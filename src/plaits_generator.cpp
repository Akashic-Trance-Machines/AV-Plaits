//
// plaits_generator.cpp
//
// AV-Plaits — ISoundGenerator wrapper for Mutable Instruments Plaits.
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of AV-Plaits and is licensed under GPL-3.0.
// See ../LICENSE.
//
#include "plaits_generator.h"

#include "stmlib/utils/buffer_allocator.h"
#include "engine/imodule.h"
#include <cstring>

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

static const TParamDesc s_Params[NUM_PARAMS] =
{
	//  pId            pLabel         Type               Display              fMin   fMax   fDef  fStep  ppOpt          nOpt
	{ "engine",       "Engine",      ParamType::Enum,   ParamDisplay::Raw,   0.0f,  23.0f, 0.0f, 1.0f, s_EngineNames, NUM_ENGINES },
	{ "harmonics",    "Harmonics",   ParamType::Float, ParamDisplay::Percent, 0.0f, 1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "timbre",       "Timbre",      ParamType::Float, ParamDisplay::Percent, 0.0f, 1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "morph",        "Morph",       ParamType::Float, ParamDisplay::Percent, 0.0f, 1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "decay",        "Decay",       ParamType::Float, ParamDisplay::Percent, 0.0f, 1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "lpg_colour",   "LPG Colour",  ParamType::Float, ParamDisplay::Percent, 0.0f, 1.0f, 0.5f, 0.01f, nullptr, 0 },
	{ "fm_amt",       "FM Amount",   ParamType::Float, ParamDisplay::Percent, 0.0f, 1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "timbre_mod",   "Timbre Mod",  ParamType::Float, ParamDisplay::Percent, 0.0f, 1.0f, 0.0f, 0.01f, nullptr, 0 },
	{ "morph_mod",    "Morph Mod",   ParamType::Float, ParamDisplay::Percent, 0.0f, 1.0f, 0.0f, 0.01f, nullptr, 0 },
};

// Null descriptor returned for out-of-range index.
static const TParamDesc s_NullParam = { "", "", ParamType::Float, ParamDisplay::Raw, 0, 0, 0, 0, nullptr, 0 };

// ── Constructor / Destructor ─────────────────────────────────────────────────

CPlaitsGenerator::CPlaitsGenerator ()
:	m_fLiveMod {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
	m_bLiveModActive (false),
	m_nCurrentNote (60),
	m_fVelocity (0.0f),
	m_bGateOpen (false),
	m_bTriggerArmed (false),
	m_fPitchBend (0.0f)
{
	memset (&m_Patch, 0, sizeof (m_Patch));
	memset (&m_Modulations, 0, sizeof (m_Modulations));
	memset (m_VoiceMem, 0, sizeof (m_VoiceMem));
}

CPlaitsGenerator::~CPlaitsGenerator () {}

void CPlaitsGenerator::Init (unsigned /*nSampleRate*/, unsigned /*nMaxBlock*/)
{
	stmlib::BufferAllocator allocator;
	allocator.Init (m_VoiceMem, sizeof (m_VoiceMem));
	m_Voice.Init (&allocator);

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
}

void CPlaitsGenerator::Reset ()
{
	m_bGateOpen     = false;
	m_bTriggerArmed = false;
	m_fVelocity     = 0.0f;
	m_nCurrentNote  = 60;
	m_fPitchBend    = 0.0f;
	// Re-init voice to clear all internal DSP state.
	stmlib::BufferAllocator allocator;
	allocator.Init (m_VoiceMem, sizeof (m_VoiceMem));
	m_Voice.Init (&allocator);
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

// ── Live modulation (called from the mod router in the main loop) ─────────────

void CPlaitsGenerator::SetLiveModulations (float fTimbre, float fMorph, float fHarmonics,
					    float fFMAmt,  float fLPGCol)
{
	m_fLiveMod[0] = fTimbre;
	m_fLiveMod[1] = fMorph;
	m_fLiveMod[2] = fHarmonics;
	m_fLiveMod[3] = fFMAmt;
	m_fLiveMod[4] = fLPGCol;
	m_bLiveModActive = true;
}

void CPlaitsGenerator::ClearLiveModulations ()
{
	m_bLiveModActive = false;
}

// ── ISoundGenerator: MIDI ────────────────────────────────────────────────────

void CPlaitsGenerator::NoteOn (uint8_t nNote, uint8_t nVelocity)
{
	if (nVelocity == 0)
	{
		NoteOff (nNote, 0);
		return;
	}
	m_nCurrentNote  = nNote;
	m_fVelocity     = (float) nVelocity / 127.0f;
	m_bGateOpen     = true;
	m_bTriggerArmed = true;		// will emit rising-edge pulse in next Process()
}

void CPlaitsGenerator::NoteOff (uint8_t nNote, uint8_t /*nVelocity*/)
{
	if (nNote == m_nCurrentNote)
		m_bGateOpen = false;
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
void CPlaitsGenerator::AllNotesOff () { m_bGateOpen = false; m_bTriggerArmed = false; m_fVelocity = 0.0f; }

// ── ISoundGenerator: audio render ────────────────────────────────────────────

void CPlaitsGenerator::Process (float *pOutL, float *pOutR, unsigned nFrames)
{
	// Plaits renders in blocks of kBlockSize (12). We call it repeatedly
	// to fill the host's 256-frame block.
	static constexpr unsigned kPlaitsBlock = plaits::kBlockSize;

	// Update the note in the persistent patch (dynamic state, not a user param).
	m_Patch.note = (float) m_nCurrentNote + m_fPitchBend;

	// Make a render copy and apply any live mod-router offsets.
	// We never mutate m_Patch with mod values so the preset is preserved.
	plaits::Patch patch = m_Patch;
	if (m_bLiveModActive)
	{
		auto c = [](float v) -> float { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; };
		patch.timbre     = c (patch.timbre     + m_fLiveMod[0]);
		patch.morph      = c (patch.morph      + m_fLiveMod[1]);
		patch.harmonics  = c (patch.harmonics  + m_fLiveMod[2]);
		patch.frequency_modulation_amount = c (patch.frequency_modulation_amount + m_fLiveMod[3]);
		patch.lpg_colour = c (patch.lpg_colour + m_fLiveMod[4]);
	}

	// Set up modulations for this block.
	m_Modulations.note      = 0.0f;
	m_Modulations.frequency = 0.0f;
	m_Modulations.harmonics = 0.0f;
	m_Modulations.timbre    = 0.0f;
	m_Modulations.morph     = 0.0f;
	// Trigger: Plaits expects a rising-edge pulse (0→1) for attack.
	// Armed on NoteOn, fired once, then held at 0 while gate stays open.
	// Level acts as VCA: velocity while gate open, 0 when released.
	m_Modulations.trigger = m_bTriggerArmed ? 1.0f : 0.0f;
	m_Modulations.level   = m_bGateOpen ? m_fVelocity : 0.0f;
	m_bTriggerArmed = false;	// pulse consumed

	unsigned written = 0;
	while (written < nFrames)
	{
		unsigned chunk = nFrames - written;
		if (chunk > kPlaitsBlock)
			chunk = kPlaitsBlock;

		plaits::Voice::Frame frames[kPlaitsBlock];
		m_Voice.Render (patch, m_Modulations, frames, chunk);

		// After the first sub-block, trigger must stay low (edge consumed).
		m_Modulations.trigger = 0.0f;

		// Convert int16 to float [-1, 1].
		// Plaits' out = main engine, aux = alternate output.
		// Send main to both L+R (mono); mix a touch of aux for stereo width.
		static constexpr float kScale    = 1.0f / 32768.0f;
		static constexpr float kAuxBlend = 0.2f;
		for (unsigned i = 0; i < chunk; i++)
		{
			float main = (float) frames[i].out * kScale;
			float aux  = (float) frames[i].aux * kScale;
			pOutL[written + i] = main + aux * kAuxBlend;
			pOutR[written + i] = main - aux * kAuxBlend;
		}
		written += chunk;
	}
}
