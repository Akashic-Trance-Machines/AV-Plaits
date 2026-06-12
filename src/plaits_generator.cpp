//
// plaits_generator.cpp
//
// AV-Plaits — ISoundGenerator wrapper for Mutable Instruments Plaits.
// Polyphonic voice pool + pull-style mod routing.
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of AV-Plaits and is licensed under GPL-3.0.
// See ../LICENSE.
//
#include "plaits_generator.h"

#include "stmlib/utils/buffer_allocator.h"
#include "engine/imodule.h"
#include <cstring>
#include <cmath>

// Factory function expected by CModuleRegistry (gen_menus.py generates the extern).
IModule *Create_plaits () { return new CPlaitsGenerator (); }

// ── Param descriptors (matches menu.json ids) ────────────────────────────────
enum ParamId : unsigned
{
	P_ENGINE = 0,
	P_HARMONICS,
	P_TIMBRE,
	P_MORPH,
	P_DECAY,
	P_LPG_COLOUR,
	P_FM_AMT,
	P_TIMBRE_MOD,
	P_MORPH_MOD,
	P_VOICES,
	P_MONO_TRIG,
	P_SRC_TIMBRE,
	P_AMT_TIMBRE,
	P_SRC_MORPH,
	P_AMT_MORPH,
	P_SRC_HARM,
	P_AMT_HARM,
	P_SRC_FM,
	P_AMT_FM,
	P_SRC_LPG,
	P_AMT_LPG,
	NUM_PARAMS
};

// Engine names in Voice::Init() registration order (see plaits/dsp/voice.cc).
// The extended-firmware engines (VA VCF … Chiptune) are registered FIRST at
// indices 0-7; the classic 16 engines follow at indices 8-23 — 24 total.
// six_op_engine_ is registered three times with different FM algorithm banks.
static const char *const s_EngineNames[] =
{
	"VA VCF",		//  0 virtual_analog_vcf_engine_
	"Phase Dist",		//  1 phase_distortion_engine_
	"6-op FM A",		//  2 six_op_engine_ (algo bank A — fm_patches_table[0])
	"6-op FM B",		//  3 six_op_engine_ (algo bank B — fm_patches_table[1])
	"6-op FM C",		//  4 six_op_engine_ (algo bank C — fm_patches_table[2])
	"Wave Terrain",		//  5 wave_terrain_engine_
	"Str Machine",		//  6 string_machine_engine_
	"Chiptune",		//  7 chiptune_engine_
	"VA Analog",		//  8 virtual_analog_engine_
	"Waveshaper",		//  9 waveshaping_engine_
	"FM 2-op",		// 10 fm_engine_
	"Grain",		// 11 grain_engine_
	"Additive",		// 12 additive_engine_
	"Wavetable",		// 13 wavetable_engine_
	"Chord",		// 14 chord_engine_
	"Speech",		// 15 speech_engine_
	"Swarm",		// 16 swarm_engine_
	"Filt Noise",		// 17 noise_engine_
	"Particle",		// 18 particle_engine_
	"String",		// 19 string_engine_
	"Modal",		// 20 modal_engine_
	"Bass Drum",		// 21 bass_drum_engine_
	"Snare Drum",		// 22 snare_drum_engine_
	"Hi-Hat",		// 23 hi_hat_engine_
};
static constexpr uint16_t NUM_ENGINES = 24;

static const char *const s_VoicesNames[]   = { "Mono", "2", "3", "4", "5", "6", "7", "8" };
static const char *const s_MonoTrigNames[] = { "Legato", "Retrig" };
static const char *const s_ModSrcNames[]   = { "None", "LFO1", "LFO2", "Env1", "Env2" };

static const TParamDesc s_Params[NUM_PARAMS] =
{
	//  pId             pLabel          Type               Display                fMin   fMax   fDef  fStep  ppOpt           nOpt
	{ "engine",        "Engine",       ParamType::Enum,  ParamDisplay::Raw,      0.0f, 23.0f, 0.0f, 1.0f,  s_EngineNames,   NUM_ENGINES },
	{ "harmonics",     "Harmonics",    ParamType::Float, ParamDisplay::Percent,  0.0f,  1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "timbre",        "Timbre",       ParamType::Float, ParamDisplay::Percent,  0.0f,  1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "morph",         "Morph",        ParamType::Float, ParamDisplay::Percent,  0.0f,  1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "decay",         "Decay",        ParamType::Float, ParamDisplay::Percent,  0.0f,  1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "lpg_colour",    "LPG Colour",   ParamType::Float, ParamDisplay::Percent,  0.0f,  1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "fm_amt",        "FM Amount",    ParamType::Float, ParamDisplay::Percent,  0.0f,  1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "timbre_mod",    "Timbre Mod",   ParamType::Float, ParamDisplay::Percent,  0.0f,  1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "morph_mod",     "Morph Mod",    ParamType::Float, ParamDisplay::Percent,  0.0f,  1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "voices",        "Voices",       ParamType::Enum,  ParamDisplay::Raw,      0.0f,  7.0f, 7.0f, 1.0f,  s_VoicesNames,   8 },
	{ "mono_trig",     "Mono Trig",    ParamType::Enum,  ParamDisplay::Raw,      0.0f,  1.0f, 0.0f, 1.0f,  s_MonoTrigNames, 2 },
	{ "mod_src_timbre","Timbre Src",   ParamType::Enum,  ParamDisplay::Raw,      0.0f,  4.0f, 0.0f, 1.0f,  s_ModSrcNames,   5 },
	{ "mod_amt_timbre","Timbre Amt",   ParamType::Float, ParamDisplay::Percent, -1.0f,  1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "mod_src_morph", "Morph Src",    ParamType::Enum,  ParamDisplay::Raw,      0.0f,  4.0f, 0.0f, 1.0f,  s_ModSrcNames,   5 },
	{ "mod_amt_morph", "Morph Amt",    ParamType::Float, ParamDisplay::Percent, -1.0f,  1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "mod_src_harm",  "Harm Src",     ParamType::Enum,  ParamDisplay::Raw,      0.0f,  4.0f, 0.0f, 1.0f,  s_ModSrcNames,   5 },
	{ "mod_amt_harm",  "Harm Amt",     ParamType::Float, ParamDisplay::Percent, -1.0f,  1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "mod_src_fm",    "FM Src",       ParamType::Enum,  ParamDisplay::Raw,      0.0f,  4.0f, 0.0f, 1.0f,  s_ModSrcNames,   5 },
	{ "mod_amt_fm",    "FM Amt",       ParamType::Float, ParamDisplay::Percent, -1.0f,  1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "mod_src_lpg",   "LPG Src",      ParamType::Enum,  ParamDisplay::Raw,      0.0f,  4.0f, 0.0f, 1.0f,  s_ModSrcNames,   5 },
	{ "mod_amt_lpg",   "LPG Amt",      ParamType::Float, ParamDisplay::Percent, -1.0f,  1.0f, 0.0f, 0.01f, nullptr, 0 },
};

// Null descriptor returned for out-of-range index.
static const TParamDesc s_NullParam = { "", "", ParamType::Float, ParamDisplay::Raw, 0, 0, 0, 0, nullptr, 0 };

// ── Constructor / Destructor ─────────────────────────────────────────────────

CPlaitsGenerator::CPlaitsGenerator ()
:	m_nAgeCounter (0),
	m_nSampleRate (48000),
	m_nPoly (MAX_VOICES),
	m_bMonoRetrig (false),
	m_fOutScale (1.0f),
	m_nHeld (0),
	m_fPitchBend (0.0f)
{
	memset (&m_Patch, 0, sizeof (m_Patch));
	memset (&m_Modulations, 0, sizeof (m_Modulations));
	memset (m_VoiceMem, 0, sizeof (m_VoiceMem));
	memset (m_HeldNotes, 0, sizeof (m_HeldNotes));
	for (unsigned i = 0; i < MAX_VOICES; i++)
	{
		m_Voice[i].nNote       = 60;
		m_Voice[i].fVelocity   = 0.0f;
		m_Voice[i].bGate       = false;
		m_Voice[i].bTrigArmed  = false;
		m_Voice[i].bActive     = false;
		m_Voice[i].nAge        = 0;
		m_Voice[i].nTailFrames = 0;
	}
	for (unsigned d = 0; d < NUM_MOD_DESTS; d++)
	{
		m_nModSrc[d] = 0;
		m_fModAmt[d] = 0.0f;
	}
	for (unsigned s = 0; s < 4; s++)
		m_fModSrcVal[s] = 0.0f;
}

CPlaitsGenerator::~CPlaitsGenerator () {}

void CPlaitsGenerator::Init (unsigned nSampleRate, unsigned /*nMaxBlock*/)
{
	m_nSampleRate = nSampleRate ? nSampleRate : 48000;

	for (unsigned i = 0; i < MAX_VOICES; i++)
	{
		stmlib::BufferAllocator allocator;
		allocator.Init (m_VoiceMem[i], VOICE_MEM_SIZE);
		m_Voice[i].Voice.Init (&allocator);
	}

	// Defaults
	m_Patch.engine		= 0;
	m_Patch.note		= 48.0f;
	m_Patch.harmonics	= 0.5f;
	m_Patch.timbre		= 0.5f;
	m_Patch.morph		= 0.5f;
	m_Patch.frequency_modulation_amount = 0.0f;
	m_Patch.timbre_modulation_amount    = 0.0f;
	m_Patch.morph_modulation_amount     = 0.0f;
	m_Patch.decay		= 0.5f;
	m_Patch.lpg_colour	= 0.5f;

	m_Modulations.frequency_patched = true;
	m_Modulations.timbre_patched    = false;
	m_Modulations.morph_patched     = false;
	m_Modulations.trigger_patched   = true;
	m_Modulations.level_patched     = true;

	m_fOutScale = 1.0f / sqrtf ((float) m_nPoly);
}

void CPlaitsGenerator::Reset ()
{
	m_fPitchBend = 0.0f;
	m_nHeld      = 0;
	for (unsigned i = 0; i < MAX_VOICES; i++)
	{
		TVoice &v = m_Voice[i];
		v.bGate = v.bTrigArmed = v.bActive = false;
		v.fVelocity = 0.0f;
		v.nTailFrames = 0;
		// Re-init voice to clear all internal DSP state.
		stmlib::BufferAllocator allocator;
		allocator.Init (m_VoiceMem[i], VOICE_MEM_SIZE);
		v.Voice.Init (&allocator);
	}
}

// ── IModule: params ──────────────────────────────────────────────────────────

unsigned CPlaitsGenerator::NumParams () const { return NUM_PARAMS; }

const TParamDesc &CPlaitsGenerator::ParamDesc (unsigned nIndex) const
{
	return (nIndex < NUM_PARAMS) ? s_Params[nIndex] : s_NullParam;
}

TParamValue CPlaitsGenerator::GetParam (unsigned nIndex) const
{
	TParamValue v = { 0.0f };
	switch (nIndex)
	{
		case P_ENGINE:		v.f = (float) m_Patch.engine; break;
		case P_HARMONICS:	v.f = m_Patch.harmonics; break;
		case P_TIMBRE:		v.f = m_Patch.timbre; break;
		case P_MORPH:		v.f = m_Patch.morph; break;
		case P_DECAY:		v.f = m_Patch.decay; break;
		case P_LPG_COLOUR:	v.f = m_Patch.lpg_colour; break;
		case P_FM_AMT:		v.f = m_Patch.frequency_modulation_amount; break;
		case P_TIMBRE_MOD:	v.f = m_Patch.timbre_modulation_amount; break;
		case P_MORPH_MOD:	v.f = m_Patch.morph_modulation_amount; break;
		case P_VOICES:		v.f = (float) (m_nPoly - 1); break;
		case P_MONO_TRIG:	v.f = m_bMonoRetrig ? 1.0f : 0.0f; break;
		case P_SRC_TIMBRE:	v.f = (float) m_nModSrc[0]; break;
		case P_AMT_TIMBRE:	v.f = m_fModAmt[0]; break;
		case P_SRC_MORPH:	v.f = (float) m_nModSrc[1]; break;
		case P_AMT_MORPH:	v.f = m_fModAmt[1]; break;
		case P_SRC_HARM:	v.f = (float) m_nModSrc[2]; break;
		case P_AMT_HARM:	v.f = m_fModAmt[2]; break;
		case P_SRC_FM:		v.f = (float) m_nModSrc[3]; break;
		case P_AMT_FM:		v.f = m_fModAmt[3]; break;
		case P_SRC_LPG:		v.f = (float) m_nModSrc[4]; break;
		case P_AMT_LPG:		v.f = m_fModAmt[4]; break;
	}
	return v;
}

void CPlaitsGenerator::SetParam (unsigned nIndex, TParamValue Value)
{
	switch (nIndex)
	{
		case P_ENGINE:		m_Patch.engine = Value.AsInt (); break;
		case P_HARMONICS:	m_Patch.harmonics = Value.f; break;
		case P_TIMBRE:		m_Patch.timbre    = Value.f; break;
		case P_MORPH:		m_Patch.morph     = Value.f; break;
		case P_DECAY:		m_Patch.decay     = Value.f; break;
		case P_LPG_COLOUR:	m_Patch.lpg_colour = Value.f; break;
		case P_FM_AMT:		m_Patch.frequency_modulation_amount = Value.f; break;
		case P_TIMBRE_MOD:	m_Patch.timbre_modulation_amount    = Value.f; break;
		case P_MORPH_MOD:	m_Patch.morph_modulation_amount     = Value.f; break;

		case P_VOICES:
		{
			unsigned nNew = (unsigned) Value.AsInt () + 1;
			if (nNew < 1) nNew = 1;
			if (nNew > MAX_VOICES) nNew = MAX_VOICES;
			if (nNew != m_nPoly)
			{
				m_nPoly = nNew;
				m_fOutScale = 1.0f / sqrtf ((float) m_nPoly);
				m_nHeld = 0;
				// Release voices outside the new pool so they fade out.
				for (unsigned i = m_nPoly; i < MAX_VOICES; i++)
					if (m_Voice[i].bActive && m_Voice[i].bGate)
						ReleaseVoice (m_Voice[i]);
			}
			break;
		}
		case P_MONO_TRIG:	m_bMonoRetrig = Value.AsInt () != 0; break;
		case P_SRC_TIMBRE:	m_nModSrc[0] = (uint8_t) Value.AsInt (); break;
		case P_AMT_TIMBRE:	m_fModAmt[0] = Value.f; break;
		case P_SRC_MORPH:	m_nModSrc[1] = (uint8_t) Value.AsInt (); break;
		case P_AMT_MORPH:	m_fModAmt[1] = Value.f; break;
		case P_SRC_HARM:	m_nModSrc[2] = (uint8_t) Value.AsInt (); break;
		case P_AMT_HARM:	m_fModAmt[2] = Value.f; break;
		case P_SRC_FM:		m_nModSrc[3] = (uint8_t) Value.AsInt (); break;
		case P_AMT_FM:		m_fModAmt[3] = Value.f; break;
		case P_SRC_LPG:		m_nModSrc[4] = (uint8_t) Value.AsInt (); break;
		case P_AMT_LPG:		m_fModAmt[4] = Value.f; break;
	}
}

int CPlaitsGenerator::FindParam (const char *pId) const
{
	for (unsigned i = 0; i < NUM_PARAMS; i++)
		if (strcmp (s_Params[i].pId, pId) == 0)
			return (int) i;
	return -1;
}

size_t CPlaitsGenerator::Serialize (uint8_t *, size_t) const { return 0; }
size_t CPlaitsGenerator::Deserialize (const uint8_t *, size_t) { return 0; }

// ── Mod sources (pushed from the mod router in the main loop) ─────────────────

void CPlaitsGenerator::SetModSources (float fLFO1, float fLFO2, float fEnv1, float fEnv2)
{
	m_fModSrcVal[0] = fLFO1;
	m_fModSrcVal[1] = fLFO2;
	m_fModSrcVal[2] = fEnv1;
	m_fModSrcVal[3] = fEnv2;
}

// ── Voice pool helpers ────────────────────────────────────────────────────────

int CPlaitsGenerator::TailFrames () const
{
	// Release tail scales with the decay parameter: 0.1 s .. 3.0 s.
	float fSeconds = 0.1f + m_Patch.decay * 2.9f;
	return (int) (fSeconds * (float) m_nSampleRate);
}

void CPlaitsGenerator::StartVoice (TVoice &v, uint8_t nNote, float fVel)
{
	v.nNote       = nNote;
	v.fVelocity   = fVel;
	v.bGate       = true;
	v.bTrigArmed  = true;
	v.bActive     = true;
	v.nAge        = ++m_nAgeCounter;
	v.nTailFrames = 0;
}

void CPlaitsGenerator::ReleaseVoice (TVoice &v)
{
	v.bGate       = false;
	v.nTailFrames = TailFrames ();
}

// Released-first, then oldest. Only voices [0..m_nPoly) participate.
int CPlaitsGenerator::AllocVoice ()
{
	int nFree = -1, nReleased = -1, nOldest = -1;
	uint32_t nReleasedAge = 0xFFFFFFFF, nOldestAge = 0xFFFFFFFF;

	for (unsigned i = 0; i < m_nPoly; i++)
	{
		TVoice &v = m_Voice[i];
		if (!v.bActive)
		{
			nFree = (int) i;
			break;
		}
		if (!v.bGate && v.nAge < nReleasedAge)
		{
			nReleased = (int) i;
			nReleasedAge = v.nAge;
		}
		if (v.nAge < nOldestAge)
		{
			nOldest = (int) i;
			nOldestAge = v.nAge;
		}
	}

	if (nFree >= 0)     return nFree;
	if (nReleased >= 0) return nReleased;
	return nOldest;
}

// ── Mono note stack ───────────────────────────────────────────────────────────

void CPlaitsGenerator::HeldPush (uint8_t nNote)
{
	HeldRemove (nNote);			// no duplicates
	if (m_nHeld >= MAX_HELD)
	{
		// Stack full: drop the oldest entry.
		memmove (m_HeldNotes, m_HeldNotes + 1, MAX_HELD - 1);
		m_nHeld = MAX_HELD - 1;
	}
	m_HeldNotes[m_nHeld++] = nNote;
}

void CPlaitsGenerator::HeldRemove (uint8_t nNote)
{
	for (unsigned i = 0; i < m_nHeld; i++)
	{
		if (m_HeldNotes[i] == nNote)
		{
			memmove (m_HeldNotes + i, m_HeldNotes + i + 1, m_nHeld - i - 1);
			m_nHeld--;
			return;
		}
	}
}

// ── ISoundGenerator: MIDI ────────────────────────────────────────────────────

void CPlaitsGenerator::NoteOn (uint8_t nNote, uint8_t nVelocity)
{
	if (nVelocity == 0)
	{
		NoteOff (nNote, 0);
		return;
	}
	float fVel = (float) nVelocity / 127.0f;

	if (m_nPoly == 1)
	{
		// ── Mono: last-note priority ─────────────────────────────────
		TVoice &v = m_Voice[0];
		bool bLegato = v.bActive && v.bGate && !m_bMonoRetrig;
		HeldPush (nNote);
		if (bLegato)
		{
			// Slide to the new note without retriggering.
			v.nNote     = nNote;
			v.fVelocity = fVel;
			v.nAge      = ++m_nAgeCounter;
		}
		else
		{
			StartVoice (v, nNote, fVel);
		}
		return;
	}

	// ── Poly ──────────────────────────────────────────────────────────────
	// Same note already sounding with gate open → retrigger that voice.
	for (unsigned i = 0; i < m_nPoly; i++)
	{
		TVoice &v = m_Voice[i];
		if (v.bActive && v.bGate && v.nNote == nNote)
		{
			StartVoice (v, nNote, fVel);
			return;
		}
	}

	int idx = AllocVoice ();
	if (idx >= 0)
		StartVoice (m_Voice[idx], nNote, fVel);
}

void CPlaitsGenerator::NoteOff (uint8_t nNote, uint8_t /*nVelocity*/)
{
	if (m_nPoly == 1)
	{
		// ── Mono: fall back to a still-held earlier note ─────────────
		HeldRemove (nNote);
		TVoice &v = m_Voice[0];
		if (!v.bActive || !v.bGate || v.nNote != nNote)
			return;
		if (m_nHeld > 0)
		{
			uint8_t nPrev = m_HeldNotes[m_nHeld - 1];
			if (m_bMonoRetrig)
				StartVoice (v, nPrev, v.fVelocity);
			else
				v.nNote = nPrev;	// legato fall-back
		}
		else
		{
			ReleaseVoice (v);
		}
		return;
	}

	for (unsigned i = 0; i < m_nPoly; i++)
	{
		TVoice &v = m_Voice[i];
		if (v.bActive && v.bGate && v.nNote == nNote)
		{
			ReleaseVoice (v);
			return;
		}
	}
}

void CPlaitsGenerator::ControlChange (uint8_t nCC, uint8_t nValue)
{
	float fNorm = (float) nValue / 127.0f;
	switch (nCC)
	{
		case 1:   m_Patch.timbre    = fNorm; break;	// Mod wheel -> timbre
		case 2:   m_Patch.morph     = fNorm; break;	// Breath -> morph
		case 74:  m_Patch.harmonics = fNorm; break;	// CC74 -> harmonics
		case 75:  m_Patch.decay     = fNorm; break;
		case 76:  m_Patch.lpg_colour = fNorm; break;
	}
}

void CPlaitsGenerator::PitchBend (int nValue14)
{
	// nValue14 = -8192..+8191 -> +/-2 semitones
	m_fPitchBend = (float) nValue14 / 8192.0f * 2.0f;
}

void CPlaitsGenerator::ChannelPressure (uint8_t /*nValue*/) {}

void CPlaitsGenerator::AllNotesOff ()
{
	m_nHeld = 0;
	for (unsigned i = 0; i < MAX_VOICES; i++)
		if (m_Voice[i].bActive && m_Voice[i].bGate)
			ReleaseVoice (m_Voice[i]);
}

// ── ISoundGenerator: audio render ────────────────────────────────────────────

void CPlaitsGenerator::Process (float *pOutL, float *pOutR, unsigned nFrames)
{
	// Plaits renders in blocks of kBlockSize (12). We call it repeatedly
	// per voice to fill the host's 256-frame block, accumulating into the
	// output buffers.
	static constexpr unsigned kPlaitsBlock = plaits::kBlockSize;

	// ── Build the routed patch for this block ────────────────────────────
	// Copy, never mutate m_Patch with mod values — presets stay clean.
	plaits::Patch patch = m_Patch;
	{
		auto c = [](float v) -> float { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; };
		float *dest[NUM_MOD_DESTS] =
		{
			&patch.timbre, &patch.morph, &patch.harmonics,
			&patch.frequency_modulation_amount, &patch.lpg_colour
		};
		for (unsigned d = 0; d < NUM_MOD_DESTS; d++)
		{
			unsigned src = m_nModSrc[d];
			if (src >= 1 && src <= 4)
				*dest[d] = c (*dest[d] + m_fModSrcVal[src - 1] * m_fModAmt[d]);
		}
	}

	// Clear output; voices accumulate.
	memset (pOutL, 0, nFrames * sizeof (float));
	memset (pOutR, 0, nFrames * sizeof (float));

	static constexpr float kScale    = 1.0f / 32768.0f;
	static constexpr float kAuxBlend = 0.2f;
	const float fGain = m_fOutScale * kScale;

	for (unsigned nv = 0; nv < MAX_VOICES; nv++)
	{
		TVoice &v = m_Voice[nv];
		if (!v.bActive)
			continue;

		patch.note = (float) v.nNote + m_fPitchBend;

		m_Modulations.note      = 0.0f;
		m_Modulations.frequency = 0.0f;
		m_Modulations.harmonics = 0.0f;
		m_Modulations.timbre    = 0.0f;
		m_Modulations.morph     = 0.0f;
		// Trigger: rising-edge pulse (0→1) for attack; level acts as VCA.
		m_Modulations.trigger = v.bTrigArmed ? 1.0f : 0.0f;
		m_Modulations.level   = v.bGate ? v.fVelocity : 0.0f;
		v.bTrigArmed = false;

		unsigned written = 0;
		while (written < nFrames)
		{
			unsigned chunk = nFrames - written;
			if (chunk > kPlaitsBlock)
				chunk = kPlaitsBlock;

			plaits::Voice::Frame frames[kPlaitsBlock];
			v.Voice.Render (patch, m_Modulations, frames, chunk);

			// After the first sub-block, trigger stays low (edge consumed).
			m_Modulations.trigger = 0.0f;

			// out = main engine, aux = alternate; mono main + aux width.
			for (unsigned i = 0; i < chunk; i++)
			{
				float main = (float) frames[i].out * fGain;
				float aux  = (float) frames[i].aux * fGain;
				pOutL[written + i] += main + aux * kAuxBlend;
				pOutR[written + i] += main - aux * kAuxBlend;
			}
			written += chunk;
		}

		// Release tail countdown → free the voice once it has faded.
		if (!v.bGate)
		{
			v.nTailFrames -= (int) nFrames;
			if (v.nTailFrames <= 0)
				v.bActive = false;
		}
	}
}
