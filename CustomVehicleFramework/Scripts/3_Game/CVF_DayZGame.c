modded class DayZGame
{
	override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
	{
		if (!g_Game)
		{
			super.OnRPC(sender, target, rpc_type, ctx);
			return;
		}

		if (g_Game.IsServer())
		{
			if (rpc_type == CVF_Constants.RPC_CLIENT_STATUS)
			{
				Param4<int, int, int, string> status = new Param4<int, int, int, string>(CVF_ClientSyncStatus.CVF_SYNC_ERROR, 0, 0, "");
				if (ctx.Read(status))
					CVF_NetworkSync.HandleClientStatus(sender, status.param1, status.param2, status.param3, status.param4);
				return;
			}

			super.OnRPC(sender, target, rpc_type, ctx);
			return;
		}

		if (rpc_type == CVF_Constants.RPC_SYNC_HELLO)
		{
			Param8<string, int, int, int, int, bool, int, int> hello = new Param8<string, int, int, int, int, bool, int, int>("", 0, 0, 0, 0, false, 0, 0);
			if (ctx.Read(hello))
				CVF_ClientNetworkSync.ReceiveHello(hello.param1, hello.param2, hello.param3, hello.param4, hello.param5, hello.param6, hello.param7, hello.param8);
			return;
		}

		if (rpc_type == CVF_Constants.RPC_PACKAGE_CHUNK)
		{
			Param4<int, int, int, string> chunk = new Param4<int, int, int, string>(0, 0, 0, "");
			if (ctx.Read(chunk))
				CVF_ClientNetworkSync.ReceivePackageChunk(chunk.param1, chunk.param2, chunk.param3, chunk.param4);
			return;
		}

		if (rpc_type == CVF_Constants.RPC_SYNC_NOTICE)
		{
			Param2<int, string> notice = new Param2<int, string>(0, "");
			if (ctx.Read(notice))
				CVF_ClientNetworkSync.ReceiveNotice(notice.param1, notice.param2);
			return;
		}

		super.OnRPC(sender, target, rpc_type, ctx);
	}

	override void ConnectFromServerBrowser(string ip, int port, string password = "")
	{
		if (!IsServer() && CVF_ClientCache.TryPrepareCachedEndpoint(ip, port))
			return;
		super.ConnectFromServerBrowser(ip, port, password);
	}

	override void ConnectFromJoin(string ip, int port)
	{
		if (!IsServer() && CVF_ClientCache.TryPrepareCachedEndpoint(ip, port))
		{
			MainMenuLaunch();
			return;
		}
		super.ConnectFromJoin(ip, port);
	}

	override void ConnectFromCLI()
	{
		string address;
		string portText;
		if (!IsServer() && GetCLIParam("connect", address))
		{
			GetCLIParam("port", portText);
			if (CVF_ClientCache.TryPrepareCachedEndpoint(address, portText.ToInt()))
			{
				MainMenuLaunch();
				return;
			}
		}
		super.ConnectFromCLI();
	}
}
