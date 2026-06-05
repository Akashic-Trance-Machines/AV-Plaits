//
// plaits_generator.h
//
// AV-Plaits — ISoundGenerator wrapper for Mutable Instruments Plaits.
// Maps MIDI input and parameter model to Plaits' Voice/Patch/Modulations.
//
// Copyright (C) 2026  The Akashic Trance Machines Team
// This file is part of AV-Plaits and is licensed under GPL-3.0.
// See ../LICENSE.
//
#pragma once

#include "engine/isoundgenerator.h"
#include "plaits/dsp/voice.h"

class CPlaitsGenerator : public ISoundGenerator
{
public:
	CPlaitsGenerator ();
	~CPlaitsGenerator () override;

	// IModule
	const char	*Id () const override	{ return "plaits"; }
	const char	*Name () const override	{ return "Plaits"; }

	unsigned	 NumParams () const override;
	const TParamDesc *ParamDesc (unsigned i) const override;
	float		 GetParam (unsigned i) const override;
	void		 SetParam (unsigned i, float fValue) override;

	bool	Serialize (uint8_t *pBuf, unsigned nMaxLen, unsigned *pLen) const override;
	bool	Deserialize (const uint8_t *pBuf, unsigned nLen) override;

	// ISoundGenerator
	void	NoteOn (uint8_t nNote, uint8_t nVelocity) override;
	void	NoteOff (uint8_t nNote, uint8_t nVelocity) override;
	void	ControlChange (uint8_t nCC, uint8_t nValue) override;
	void	PitchBend (int16_t nBend) override;
	void	Process (float *pOutL, float *pOutR, unsigned nFrames) override;

private:
	// Plaits core
	plaits::Voice		m_Voice;
	plaits::Patch		m_Patch;
	plaits::Modulations	m_Modulations;

	// Voice memory pool (Plaits uses BufferAllocator, no heap).
	static constexpr unsigned VOICE_MEM_SIZE = 65536;
	uint8_t			m_VoiceMem[VOICE_MEM_SIZE];

	// MIDI state
	uint8_t			m_nCurrentNote;
	float			m_fVelocity;
	bool			m_bGateOpen;
	float			m_fPitchBend;		// semitones (-2..+2)
};
