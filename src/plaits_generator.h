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
	ModuleKind	 Kind () const override	{ return ModuleKind::Generator; }

	void		 Init (unsigned nSampleRate, unsigned nMaxBlock) override;
	void		 Reset () override;

	unsigned		 NumParams () const override;
	const TParamDesc	&ParamDesc (unsigned nIndex) const override;
	TParamValue		 GetParam (unsigned nIndex) const override;
	void			 SetParam (unsigned nIndex, TParamValue Value) override;
	int			 FindParam (const char *pId) const override;

	size_t	Serialize (uint8_t *pBuffer, size_t nCapacity) const override;
	size_t	Deserialize (const uint8_t *pBuffer, size_t nLength) override;

	// ISoundGenerator
	void	NoteOn (uint8_t nNote, uint8_t nVelocity) override;
	void	NoteOff (uint8_t nNote, uint8_t nVelocity) override;
	void	ControlChange (uint8_t nCC, uint8_t nValue) override;
	void	PitchBend (int nValue14) override;
	void	ChannelPressure (uint8_t nValue) override;
	void	AllNotesOff () override;
	void	Process (float *pOutL, float *pOutR, unsigned nFrames) override;

	// Live modulation — called from the main-loop mod router to apply LFO /
	// cyclic-envelope offsets on top of the current preset values.
	// Written from the main loop, read from the audio callback.  Single-float
	// stores/loads on AArch64 are atomic; the worst case is one block rendered
	// with a slightly stale value, which is inaudible for smooth modulation.
	void	SetLiveModulations (float fTimbre, float fMorph, float fHarmonics,
				    float fFMAmt,  float fLPGCol);
	void	ClearLiveModulations ();

private:
	// Plaits core
	plaits::Voice		m_Voice;
	plaits::Patch		m_Patch;
	plaits::Modulations	m_Modulations;

	// Voice memory pool (Plaits uses BufferAllocator, no heap).
	static constexpr unsigned VOICE_MEM_SIZE = 65536;
	uint8_t			m_VoiceMem[VOICE_MEM_SIZE];

	// Live modulation offsets (main-loop → audio callback).
	// [0]=timbre [1]=morph [2]=harmonics [3]=fm_amt [4]=lpg_colour
	volatile float		m_fLiveMod[5];
	volatile bool		m_bLiveModActive;

	// MIDI state
	uint8_t			m_nCurrentNote;
	float			m_fVelocity;
	bool			m_bGateOpen;
	bool			m_bTriggerArmed;	// rising-edge pulse: set on NoteOn, cleared after 1st render
	float			m_fPitchBend;		// semitones (-2..+2)
};
