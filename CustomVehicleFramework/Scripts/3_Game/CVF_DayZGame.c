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
			Param6<string, int, int, int, int, bool> hello = new Param6<string, int, int, int, int, bool>("", 0, 0, 0, 0, false);
			if (ctx.Read(hello))
				CVF_ClientNetworkSync.ReceiveHello(hello.param1, hello.param2, hello.param3, hello.param4, hello.param5, hello.param6);
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
}
