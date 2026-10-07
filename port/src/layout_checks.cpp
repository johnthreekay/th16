// Layout facts the portable build depends on at every pointer size. The
// game's own static_asserts describe the 32-bit layout and are skipped in
// the 64-bit build (port_prelude.h); these stay on in both.
#include <dsound.h>
#include <mmreg.h>

#include "AnmManager.h"
#include "ReplayManager.h"
#include "Scorefile.h"
#include "SoundManager.h"
#include "Supervisor.h"

// Structures read from or written to files: no pointers, so the same size
// everywhere.
TH16_PORT_CHECK(sizeof(WAVEFORMATEX) == 18);
TH16_PORT_CHECK(sizeof(ThBgmFormat) == 0x34);
TH16_PORT_CHECK(sizeof(Config) == 0x68);
TH16_PORT_CHECK(sizeof(RpyInfo) == 0xa0);
TH16_PORT_CHECK(sizeof(RpyGamestate) == 0x294);
TH16_PORT_CHECK(sizeof(AnmRawEntry) == 0x40);
TH16_PORT_CHECK(sizeof(ScorefileChara) == 0x5318);
TH16_PORT_CHECK(sizeof(ScorefileStatus) == 0x42c);

// Scorefile and ScorefileData are two views of one object (the second one
// starts with two pointers); Scorefile.h pads the first on 64-bit.
TH16_PORT_CHECK(sizeof(ScorefileData) == sizeof(Scorefile));
TH16_PORT_CHECK(sizeof(ScorefileCharacter) == sizeof(ScorefileChara));
TH16_PORT_CHECK(offsetof(Scorefile, characters) + offsetof(ScorefileCharacter, scores) ==
                offsetof(ScorefileData, charas) + offsetof(ScorefileChara, scores));

// DSUtil.cpp copies a whole DSBUFFERDESC into CSound::m_desc.
TH16_PORT_CHECK(sizeof(((CSound *)0)->m_desc) == sizeof(DSBUFFERDESC));

// BgmStream is a view of CStreamingSound.
TH16_PORT_CHECK(offsetof(BgmStream, buffers) == offsetof(CSound, m_apDSBuffer));
TH16_PORT_CHECK(offsetof(BgmStream, wave_file) == offsetof(CSound, m_pWaveFile));
TH16_PORT_CHECK(offsetof(BgmStream, fade_time_left) == offsetof(CSound, m_fade_time_left));
TH16_PORT_CHECK(offsetof(BgmStream, fade_duration) == offsetof(CSound, m_fade_duration));
TH16_PORT_CHECK(offsetof(BgmStream, fade_mode) == offsetof(CSound, m_fade_mode));
TH16_PORT_CHECK(offsetof(BgmStream, unk_50) == offsetof(CSound, m_playing));
TH16_PORT_CHECK(offsetof(BgmStream, refilling) == offsetof(CStreamingSound, m_refilling));
