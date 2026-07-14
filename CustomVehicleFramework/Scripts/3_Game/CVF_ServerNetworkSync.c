class CVF_ServerClientSyncState
{
	int Status;
	int ExpectedHash;

	void CVF_ServerClientSyncState(int expectedHash)
	{
		Status = CVF_ClientSyncStatus.CVF_SYNC_NONE;
		ExpectedHash = expectedHash;
	}
}

class CVF_NetworkSync
{
	private static ref map<string, ref CVF_ServerClientSyncState> s_ClientStates = new map<string, ref CVF_ServerClientSyncState>;

	static void SendHelloToClient(PlayerIdentity identity)
	{
		if (!g_Game || !g_Game.IsServer() || !identity)
			return;

		GetCVFConfigManager().LoadConfig();
		if (GetCVFConfigManager().HasLoadError() || !GetCVFConfigManager().WasLastGenerationSuccessful() || !CVF_ConfigGenerator.HasGeneratedPackage())
		{
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "The server vehicle configuration could not be generated. The administrator must check the CVF server log and vehicles.json.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 3000, false, identity);
			return;
		}

		int payloadHash = CVF_ConfigGenerator.GetCurrentPayloadHash();
		int payloadChars = CVF_ConfigGenerator.GetCurrentPayloadChars();
		string serverId = GetCVFConfigManager().m_Config.ServerId;
		int loadedHash = CVF_ConfigGenerator.GetLoadedPayloadHash();
		bool ready = CVF_ConfigGenerator.IsLoadedConfigCurrent();

		s_ClientStates.Set(identity.GetId(), new CVF_ServerClientSyncState(payloadHash));
		Param6<string, int, int, int, int, bool> hello = new Param6<string, int, int, int, int, bool>(serverId, payloadHash, payloadChars, CVF_Constants.SYNC_PROTOCOL_VERSION, loadedHash, ready);
		g_Game.RPCSingleParam(null, CVF_Constants.RPC_SYNC_HELLO, hello, true, identity);

		if (!ready)
		{
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SERVER_RESTART_REQUIRED, "The server generated new vehicle overrides but has not loaded them yet. The administrator must restart the complete server process.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 3000, false, identity);
			return;
		}

		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckSyncTimeout, CVF_Constants.SYNC_TIMEOUT_MS, false, identity, payloadHash);
		CVF_Logger.Log("Sent experimental server-config handshake to " + identity.GetName() + " Hash=" + payloadHash.ToString());
	}

	static void HandleClientStatus(PlayerIdentity identity, int status, int payloadHash, int protocolVersion, string detail)
	{
		if (!g_Game || !g_Game.IsServer() || !identity)
			return;

		CVF_ServerClientSyncState state;
		if (!s_ClientStates.Find(identity.GetId(), state))
		{
			CVF_Logger.Warning("Ignored CVF status without a server hello from " + identity.GetName());
			return;
		}

		if (protocolVersion != CVF_Constants.SYNC_PROTOCOL_VERSION || payloadHash != state.ExpectedHash || payloadHash != CVF_ConfigGenerator.GetCurrentPayloadHash())
		{
			state.Status = CVF_ClientSyncStatus.CVF_SYNC_ERROR;
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "Vehicle configuration handshake mismatch.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 1500, false, identity);
			return;
		}

		if (state.Status != CVF_ClientSyncStatus.CVF_SYNC_NONE || (status != CVF_ClientSyncStatus.CVF_SYNC_READY && status != CVF_ClientSyncStatus.CVF_SYNC_ERROR))
		{
			state.Status = CVF_ClientSyncStatus.CVF_SYNC_ERROR;
			SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "Invalid vehicle configuration handshake state.");
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 1500, false, identity);
			return;
		}

		state.Status = status;
		if (status == CVF_ClientSyncStatus.CVF_SYNC_READY)
		{
			CVF_Logger.Log("CVF client script acknowledged the server override for " + identity.GetName() + ". Client-native config is not verified in server-bootstrap mode.");
			return;
		}

		CVF_Logger.Warning("Client vehicle handshake failed for " + identity.GetName() + ": " + detail);
		SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "Vehicle configuration handshake failed. Check the client script log.");
		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DisconnectClient, 2000, false, identity);
	}

	static void RemoveClient(PlayerIdentity identity)
	{
		if (identity)
			s_ClientStates.Remove(identity.GetId());
	}

	private static void SendNotice(PlayerIdentity identity, int noticeType, string message)
	{
		Param2<int, string> notice = new Param2<int, string>(noticeType, message);
		g_Game.RPCSingleParam(null, CVF_Constants.RPC_SYNC_NOTICE, notice, true, identity);
	}

	private static void CheckSyncTimeout(PlayerIdentity identity, int expectedHash)
	{
		if (!g_Game || !g_Game.IsServer() || !identity)
			return;

		CVF_ServerClientSyncState state;
		if (!s_ClientStates.Find(identity.GetId(), state))
			return;
		if (state.ExpectedHash != expectedHash || state.Status == CVF_ClientSyncStatus.CVF_SYNC_READY)
			return;

		SendNotice(identity, CVF_SyncNoticeType.CVF_NOTICE_SYNC_ERROR, "Vehicle configuration handshake timed out.");
		DisconnectClient(identity);
	}

	private static void DisconnectClient(PlayerIdentity identity)
	{
		if (!g_Game || !g_Game.IsServer() || !identity)
			return;
		s_ClientStates.Remove(identity.GetId());
		g_Game.DisconnectPlayer(identity);
	}
}
