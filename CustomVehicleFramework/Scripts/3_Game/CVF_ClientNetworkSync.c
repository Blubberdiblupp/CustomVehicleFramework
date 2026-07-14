class CVF_ClientNetworkSync
{
	private static int s_ExpectedHash = 0;

	static void ReceiveHello(string serverId, int payloadHash, int payloadChars, int protocolVersion, int loadedHash, bool serverReady)
	{
		s_ExpectedHash = payloadHash;
		if (!CVF_SharedUtils.IsSafeToken(serverId) || payloadHash == 0 || payloadChars <= 0 || payloadChars > CVF_Constants.MAX_GENERATED_PACKAGE_CHARS)
		{
			Fail("The server sent invalid vehicle override metadata.");
			return;
		}

		if (protocolVersion != CVF_Constants.SYNC_PROTOCOL_VERSION)
		{
			Fail("The server uses an incompatible Custom Vehicle Framework protocol.");
			return;
		}

		if (!serverReady || loadedHash != payloadHash)
		{
			Fail("This server generated new vehicle settings but has not loaded them yet. The server administrator must restart it again.");
			return;
		}

		CVF_Logger.Log("Server-side native vehicle overrides acknowledged. Client-native config values are not verified by this experimental mode.");
		SendStatus(CVF_ClientSyncStatus.CVF_SYNC_READY, "CVF client script present; server-side native config acknowledged.");
	}

	static void ReceiveNotice(int noticeType, string message)
	{
		CVF_ClientUI.ShowError(message);
	}

	private static void Fail(string detail)
	{
		if (detail == "")
			detail = "Unknown vehicle override handshake error.";
		CVF_ClientUI.ShowError(detail);
		SendStatus(CVF_ClientSyncStatus.CVF_SYNC_ERROR, detail);
	}

	private static void SendStatus(int status, string detail)
	{
		if (!g_Game || g_Game.IsServer() || s_ExpectedHash == 0)
			return;

		Param4<int, int, int, string> payload = new Param4<int, int, int, string>(status, s_ExpectedHash, CVF_Constants.SYNC_PROTOCOL_VERSION, detail);
		g_Game.RPCSingleParam(null, CVF_Constants.RPC_CLIENT_STATUS, payload, true, null);
	}
}
