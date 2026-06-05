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
#include <cstring>

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

static const TParamDesc s_Params[NUM_PARAMS] =
{
	{ "engine",	"Engine",	ParamType::Int,		0.0f, 23.0f,	0.0f,  1.0f },
	{ "harmonics",	"Harmonics",	ParamType::Float,	0.0f,  1.0f,	0.5f,  0.01f },
	{ "timbre",	"Timbre",	ParamType::Float,	0.0f,  1.0f,	0.5f,  0.01f },
	{ "morph",	"Morph",	ParamType::Float,	0.0f,  1.0f,	0.5f,  0.01f },
	{ "decay",	"Decay",	ParamType::Float,	0.0f,  1.0f,	0.5f,  0.01f },
	{ "lpg_colour",	"LPG Colour",	ParamType::Float,	0.0f,  1.0f,	0.5f,  0.01f },
	{ "fm_amt",	"FM Amount",	ParamType::Float,	0.0f,  1.0f,	0.0f,  0.01f },
	{ "timbre_mod",	"Timbre Mod",	ParamType::Float,	0.0f,  1.0f,	0.0f,  0.01f },
	{ "morph_mod",	"Morph Mod",	ParamType::Float,	0.0f,  1.0f,	0.0f,  0.01f },
};

// ── Constructor / Destructor ─────────────────────────────────────────────────

CPlaitsGenerator::CPlaitsGenerator ()
:	m_nCurrentNote (60),
	m_fVelocity (0.0f),
	m_bGateOpen (false),
	m_fPitchBend (0.0f)
{
	memset (&m_Patch, 0, sizeof (m_Patch));
	memset (&m_Modulations, 0, sizeof (m_Modulations));
	memset (m_VoiceMem, 0, sizeof (m_VoiceMem));

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

CPlaitsGenerator::~CPlaitsGenerator () {}

// ── IModule: params ──────────────────────────────────────────────────────────

unsigned CPlaitsGenerator::NumParams () const	{ return NUM_PARAMS; }
const TParamDesc *CPlaitsGenerator::ParamDesc (unsigned i) const
{
	return (i < NUM_PARAMS) ? &s_Params[i] : nullptr;
}

float CPlaitsGenerator::GetParam (unsigned i) const
{
	switch (i)
	{
		case P_ENGINE:		return (float) m_Patch.engine;
		case P_HARMONICS:	return m_Patch.harmonics;
		case P_TIMBRE:		return m_Patch.timbre;
		case P_MORPH:		return m_Patch.morph;
		case P_DECAY:		return m_Patch.decay;
		case P_LPG_COLOUR:	return m_Patch.lpg_colour;
		case P_FM_AMT:		return m_Patch.frequency_modulation_amount;
		case P_TIMBRE_MOD:	return m_Patch.timbre_modulation_amount;
		case P_MORPH_MOD:	return m_Patch.morph_modulation_amount;
		default:		return 0.0f;
	}
}

void CPlaitsGenerator::SetParam (unsigned i, float fValue)
{
	switch (i)
	{
		case P_ENGINE:		m_Patch.engine = (int) fValue; break;
		case P_HARMONICS:	m_Patch.harmonics = fValue; break;
		case P_TIMBRE:		m_Patch.timbre    = fValue; break;
		case P_MORPH:		m_Patch.morph     = fValue; break;
		case P_DECAY:		m_Patch.decay     = fValue; break;
		case P_LPG_COLOUR:	m_Patch.lpg_colour = fValue; break;
		case P_FM_AMT:		m_Patch.frequency_modulation_amount = fValue; break;
		case P_TIMBRE_MOD:	m_Patch.timbre_modulation_amount    = fValue; break;
		case P_MORPH_MOD:	m_Patch.morph_modulation_amount     = fValue; break;
	}
}

bool CPlaitsGenerator::Serialize (uint8_t *, unsigned, unsigned *) const { return false; }
bool CPlaitsGenerator::Deserialize (const uint8_t *, unsigned) { return false; }

// ── ISoundGenerator: MIDI ────────────────────────────────────────────────────

void CPlaitsGenerator::NoteOn (uint8_t nNote, uint8_t nVelocity)
{
	if (nVelocity == 0)
	{
		NoteOff (nNote, 0);
		return;
	}
	m_nCurrentNote = nNote;
	m_fVelocity    = (float) nVelocity / 127.0f;
	m_bGateOpen    = true;
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
		case 1:   m_Patch.timbre    = fNorm; break;	// Mod wheel → timbre
		case 2:   m_Patch.morph     = fNorm; break;	// Breath → morph
		case 74:  m_Patch.harmonics = fNorm; break;	// CC74 → harmonics
		case 75:  m_Patch.decay     = fNorm; break;
		case 76:  m_Patch.lpg_colour = fNorm; break;
	}
}

void CPlaitsGenerator::PitchBend (int16_t nBend)
{
	// nBend = -8192..+8191 → ±2 semitones
	m_fPitchBend = (float) nBend / 8192.0f * 2.0f;
}

// ── ISoundGenerator: audio render ────────────────────────────────────────────

void CPlaitsGenerator::Process (float *pOutL, float *pOutR, unsigned nFrames)
{
	// Plaits renders in blocks of kBlockSize (12). We call it repeatedly
	// to fill the host's 256-frame block.
	static constexpr unsigned kPlaitsBlock = plaits::kBlockSize;

	// Set up modulations for this block.
	m_Patch.note = (float) m_nCurrentNote + m_fPitchBend;
	m_Modulations.note     = 0.0f;
	m_Modulations.frequency = 0.0f;
	m_Modulations.harmonics = 0.0f;
	m_Modulations.timbre   = 0.0f;
	m_Modulations.morph    = 0.0f;
	m_Modulations.trigger  = m_bGateOpen ? 1.0f : 0.0f;
	m_Modulations.level    = m_bGateOpen ? m_fVelocity : 0.0f;

	unsigned written = 0;
	while (written < nFrames)
	{
		unsigned chunk = nFrames - written;
		if (chunk > kPlaitsBlock)
			chunk = kPlaitsBlock;

		plaits::Voice::Frame frames[kPlaitsBlock];
		m_Voice.Render (m_Patch, m_Modulations, frames, chunk);

		// Convert int16 to float [-1, 1], write both channels.
		static constexpr float kScale = 1.0f / 32768.0f;
		for (unsigned i = 0; i < chunk; i++)
		{
			pOutL[written + i] = (float) frames[i].out * kScale;
			pOutR[written + i] = (float) frames[i].aux * kScale;
		}
		written += chunk;
	}
}
