//
// plaits_generator.h
//
// AV-Plaits — ISoundGenerator wrapper for Mutable Instruments Plaits.
// Polyphonic (up to 8 voices) with released-first voice stealing, mono mode
// with legato/retrigger, and pull-style mod routing (the SG owns its routes;
// the kernel only feeds raw LFO/Env source values).
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
	static constexpr unsigned MAX_VOICES = 8;

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

	// Mod sources — pushed from the main-loop mod router every tick.
	// The SG pulls these into its parameters according to its own route
	// params (mod_src_* / mod_amt_*). LFO values are bipolar (±depth),
	// Env values unipolar (0..depth). Single-float stores on AArch64 are
	// atomic; one block of staleness is inaudible for smooth modulation.
	void	SetModSources (float fLFO1, float fLFO2, float fEnv1, float fEnv2) override;

private:
	// ── Voice pool ────────────────────────────────────────────────────────
	struct TVoice
	{
		plaits::Voice	Voice;
		uint8_t		nNote;
		float		fVelocity;
		bool		bGate;		// key held
		bool		bTrigArmed;	// rising-edge pulse pending
		bool		bActive;	// rendering (held or release tail)
		uint32_t	nAge;		// allocation order (steal policy)
		int		nTailFrames;	// release countdown once gate off
	};

	int	AllocVoice ();		// released-first, then oldest
	void	StartVoice (TVoice &v, uint8_t nNote, float fVel);
	void	ReleaseVoice (TVoice &v);
	int	TailFrames () const;	// release tail length from decay param

	// Per-voice DSP memory (Plaits BufferAllocator, no heap).
	static constexpr unsigned VOICE_MEM_SIZE = 65536;
	uint8_t		m_VoiceMem[MAX_VOICES][VOICE_MEM_SIZE];

	TVoice		m_Voice[MAX_VOICES];
	uint32_t	m_nAgeCounter;
	unsigned	m_nSampleRate;

	// Shared patch/modulation templates (per-voice fields set in Process).
	plaits::Patch		m_Patch;
	plaits::Modulations	m_Modulations;

	// ── Settings ──────────────────────────────────────────────────────────
	unsigned	m_nPoly;	// 1 = mono, 2..MAX_VOICES
	bool		m_bMonoRetrig;	// mono: retrigger envelope on legato notes
	float		m_fOutScale;	// 1/sqrt(poly) headroom scaling

	// ── Mono note stack (last-note priority with fall-back) ─────────────
	static constexpr unsigned MAX_HELD = 16;
	uint8_t		m_HeldNotes[MAX_HELD];
	unsigned	m_nHeld;
	void		HeldPush (uint8_t nNote);
	void		HeldRemove (uint8_t nNote);

	// ── Mod routing (pull model) ──────────────────────────────────────────
	// Destinations: 0=timbre 1=morph 2=harmonics 3=fm_amt 4=lpg_colour
	static constexpr unsigned NUM_MOD_DESTS = 5;
	uint8_t		m_nModSrc[NUM_MOD_DESTS];	// 0=None 1=LFO1 2=LFO2 3=Env1 4=Env2
	float		m_fModAmt[NUM_MOD_DESTS];	// -1..+1
	volatile float	m_fModSrcVal[4];		// LFO1, LFO2, Env1, Env2

	float		m_fPitchBend;			// semitones (-2..+2)
};
