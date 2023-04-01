#include "netplay.h"
#include "pad.h"
#include "spu.h"
#include "system.h"
#include <bitset>
#include <frontend-common/input_manager.h>

// Netplay Impl
Netplay::Session::Session() = default;

Netplay::Session::~Session()
{
  Close();
}

int32_t Netplay::Session::Start(int32_t lhandle, uint16_t lport, std::string& raddr, uint16_t rport, int32_t ldelay,
                                uint32_t pred)
{
  s_net_session.m_max_pred = pred;

  GGPOSessionCallbacks cb{};

  cb.advance_frame = NpAdvFrameCb;
  cb.save_game_state = NpSaveFrameCb;
  cb.load_game_state = NpLoadFrameCb;
  cb.begin_game = NpBeginGameCb;
  cb.free_buffer = NpFreeBuffCb;
  cb.on_event = NpOnEventCb;
  cb.log_game_state = NpLogNetplayCb;

  GGPOErrorCode result;
#ifdef SYNCTEST
  char n[10] = "synctest";
  result = ggpo_start_synctest(&s_net_session.p_ggpo, &cb, n, 2, sizeof(Netplay::Input), pred);
#else
  result = ggpo_start_session(&s_net_session.p_ggpo, &cb, "Duckstation-Netplay", 2, sizeof(Netplay::Input), lport,
                              s_net_session.m_max_pred);
#endif // SYNCTEST

  ggpo_set_disconnect_timeout(s_net_session.p_ggpo, 3000);
  ggpo_set_disconnect_notify_start(s_net_session.p_ggpo, 1000);

  for (int i = 1; i <= 2; i++)
  {
    GGPOPlayer player = {};
    GGPOPlayerHandle handle = 0;

    player.size = sizeof(GGPOPlayer);
    player.player_num = i;

    if (lhandle == i)
    {
      player.type = GGPOPlayerType::GGPO_PLAYERTYPE_LOCAL;
      result = ggpo_add_player(s_net_session.p_ggpo, &player, &handle);
      s_net_session.m_local_handle = handle;
    }
    else
    {
      player.type = GGPOPlayerType::GGPO_PLAYERTYPE_REMOTE;
#ifdef _WIN32
      strcpy_s(player.u.remote.ip_address, raddr.c_str());
#else
      strcpy(player.u.remote.ip_address, raddr.c_str());
#endif
      player.u.remote.port = rport;
      result = ggpo_add_player(s_net_session.p_ggpo, &player, &handle);
    }
  }
  ggpo_set_frame_delay(s_net_session.p_ggpo, s_net_session.m_local_handle, ldelay);

  return result;
}

int32_t Netplay::Session::StartTraversal(std::vector<uint16_t> handles, std::vector<std::string> addresses,
                                         std::vector<uint16_t> ports, int input_delay, uint32_t pred)
{
  GGPOErrorCode result;
  GGPOSessionCallbacks cb{};

  cb.advance_frame = NpAdvFrameCb;
  cb.save_game_state = NpSaveFrameCb;
  cb.load_game_state = NpLoadFrameCb;
  cb.begin_game = NpBeginGameCb;
  cb.free_buffer = NpFreeBuffCb;
  cb.on_event = NpOnEventCb;
  cb.log_game_state = NpLogNetplayCb;

  // if the first index its handle is 3 or higher. for now (31/03/2023) this means that the local client is a spectator.
  if (handles[0] >= 3)
  {
    // find player 1 to host the spectators
    int p1Idx = -1;
    for (int i = 0; i < handles.size(); i++)
      if (handles[i] == 1)
        p1Idx = i;
    // p1 not found = bad session.
    if (p1Idx == -1)
      return GGPO_ERRORCODE_INVALID_SESSION;
    // setup session
    char buff[20] = "Duckstation";
    char* addr = addresses[p1Idx].data();
    result =
      ggpo_start_spectating(&s_net_session.p_ggpo, &cb, buff, 2, sizeof(Netplay::Input), ports[0], addr, ports[p1Idx]);
    return result;
  }
  // otherwise setup a normal session and if your local is player 1 be sure to add spectators
  s_net_session.m_max_pred = pred;

  result = ggpo_start_session(&s_net_session.p_ggpo, &cb, "Duckstation-Netplay", 2, sizeof(Netplay::Input), ports[0],
                              s_net_session.m_max_pred);

  ggpo_set_disconnect_timeout(s_net_session.p_ggpo, 3000);
  ggpo_set_disconnect_notify_start(s_net_session.p_ggpo, 1000);

  for (int i = 0; i < handles.size(); i++)
  {
    GGPOPlayer player = {};
    GGPOPlayerHandle handle = 0;
    player.size = sizeof(GGPOPlayer);
    player.player_num = handles[i];

    if (i == 0)
    {
      // local
      player.type = GGPOPlayerType::GGPO_PLAYERTYPE_LOCAL;
      result = ggpo_add_player(s_net_session.p_ggpo, &player, &handle);
      s_net_session.m_local_handle = handle;
    }
    else
    {
      // remotes and spectator
      player.type = GGPOPlayerType::GGPO_PLAYERTYPE_SPECTATOR;
      if (handles[i] < 3)
        player.type = GGPOPlayerType::GGPO_PLAYERTYPE_REMOTE;
      else if (handles[0] != 1) // if local is not player 1 then dont handle spectators
        continue;
#ifdef _WIN32
      strcpy_s(player.u.remote.ip_address, addresses[i].c_str());
#else
      strcpy(player.u.remote.ip_address, addresses[i].c_str());
#endif
      player.u.remote.port = ports[i];
      result = ggpo_add_player(s_net_session.p_ggpo, &player, &handle);
    }
  }
  ggpo_set_frame_delay(s_net_session.p_ggpo, s_net_session.m_local_handle, input_delay);
  return result;
}

void Netplay::Session::Close()
{
  ggpo_close_session(s_net_session.p_ggpo);
  s_net_session.p_ggpo = nullptr;
  s_net_session.m_local_handle = GGPO_INVALID_HANDLE;
  s_net_session.m_max_pred = 0;
}

bool Netplay::Session::IsActive()
{
  return s_net_session.p_ggpo != nullptr;
}

void Netplay::Session::RunIdle()
{
  ggpo_idle(s_net_session.p_ggpo);
}

void Netplay::Session::AdvanceFrame(uint16_t checksum)
{
  ggpo_advance_frame(s_net_session.p_ggpo, checksum);
}

void Netplay::Session::RunFrame(int64_t& waitTime)
{
  // run game
  auto result = GGPO_OK;
  int disconnectFlags = 0;
  Netplay::Input inputs[2] = {};
  // add local input
  if (GetLocalHandle() != GGPO_INVALID_HANDLE)
  {
    auto inp = ReadLocalInput();
    result = AddLocalInput(inp);
  }
  // advance game
  if (GGPO_SUCCEEDED(result))
  {
    result = SyncInput(inputs, &disconnectFlags);
    if (GGPO_SUCCEEDED(result))
    {
      // enable again when rolling back done
      SPU::SetAudioOutputMuted(false);
      System::NetplayAdvanceFrame(inputs, disconnectFlags);
    }
    else
      RunIdle();
  }
  else
    RunIdle();

  waitTime = GetTimer()->UsToWaitThisLoop();
}

int32_t Netplay::Session::CurrentFrame()
{
  int32_t frame;
  ggpo_get_current_frame(s_net_session.p_ggpo, frame);
  return frame;
}

void Netplay::Session::CollectInput(uint32_t slot, uint32_t bind, float value)
{
  s_net_session.m_net_input[slot][bind] = value;
}

Netplay::Input Netplay::Session::ReadLocalInput()
{
  // get controller data of the first controller (0 internally)
  Netplay::Input inp{0};
  for (uint32_t i = 0; i < (uint32_t)DigitalController::Button::Count; i++)
  {
    if (s_net_session.m_net_input[0][i] >= 0.25f)
      inp.button_data |= 1 << i;
  }
  return inp;
}

std::string& Netplay::Session::GetGamePath()
{
  return s_net_session.m_game_path;
}

void Netplay::Session::SetGamePath(std::string& path)
{
  s_net_session.m_game_path = path;
}

void Netplay::Session::SendMsg(const char* msg)
{
  ggpo_client_chat(s_net_session.p_ggpo, msg);
}

GGPOErrorCode Netplay::Session::SyncInput(Netplay::Input inputs[2], int* disconnect_flags)
{
  return ggpo_synchronize_input(s_net_session.p_ggpo, inputs, sizeof(Netplay::Input) * 2, disconnect_flags);
}

GGPOErrorCode Netplay::Session::AddLocalInput(Netplay::Input input)
{
  return ggpo_add_local_input(s_net_session.p_ggpo, s_net_session.m_local_handle, &input, sizeof(Netplay::Input));
}

GGPONetworkStats& Netplay::Session::GetNetStats(int32_t handle)
{
  ggpo_get_network_stats(s_net_session.p_ggpo, handle, &s_net_session.m_last_net_stats);
  return s_net_session.m_last_net_stats;
}

int32_t Netplay::Session::GetPing()
{
  const int handle = GetLocalHandle() == 1 ? 2 : 1;
  ggpo_get_network_stats(s_net_session.p_ggpo, handle, &s_net_session.m_last_net_stats);
  return s_net_session.m_last_net_stats.network.ping;
}

uint32_t Netplay::Session::GetMaxPrediction()
{
  return s_net_session.m_max_pred;
}

GGPOPlayerHandle Netplay::Session::GetLocalHandle()
{
  return s_net_session.m_local_handle;
}

void Netplay::Session::SetInputs(Netplay::Input inputs[2])
{
  for (u32 i = 0; i < 2; i++)
  {
    auto cont = Pad::GetController(i);
    std::bitset<sizeof(u32) * 8> buttonBits(inputs[i].button_data);
    for (u32 j = 0; j < (u32)DigitalController::Button::Count; j++)
      cont->SetBindState(j, buttonBits.test(j) ? 1.0f : 0.0f);
  }
}

Netplay::LoopTimer* Netplay::Session::GetTimer()
{
  return &s_net_session.m_timer;
}

void Netplay::LoopTimer::Init(uint32_t fps, uint32_t frames_to_spread_wait)
{
  m_us_per_game_loop = 1000000 / fps;
  m_us_ahead = 0;
  m_us_extra_to_wait = 0;
  m_frames_to_spread_wait = frames_to_spread_wait;
  m_last_advantage = 0.0f;
}

void Netplay::LoopTimer::OnGGPOTimeSyncEvent(float frames_ahead)
{
  m_last_advantage = (1000.0f * frames_ahead / 60.0f);
  m_last_advantage /= 2;
  if (m_last_advantage < 0)
  {
    int t = 0;
    t++;
  }
  m_us_extra_to_wait = (int)(m_last_advantage * 1000);
  if (m_us_extra_to_wait)
  {
    m_us_extra_to_wait /= m_frames_to_spread_wait;
    m_wait_count = m_frames_to_spread_wait;
  }
}

int32_t Netplay::LoopTimer::UsToWaitThisLoop()
{
  int32_t timetoWait = m_us_per_game_loop;
  if (m_wait_count)
  {
    timetoWait += m_us_extra_to_wait;
    m_wait_count--;
    if (!m_wait_count)
      m_us_extra_to_wait = 0;
  }
  return timetoWait;
}
