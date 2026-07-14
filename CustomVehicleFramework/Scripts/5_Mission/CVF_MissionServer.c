modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		if (!g_Game || !g_Game.IsServer())
			return;

		CVF_Logger.Log("Server starting. Loading CustomVehicleFramework config.");
		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CVF_LoadServerConfig, 1000, false);
		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CVF_WatchServerConfig, CVF_Constants.CONFIG_WATCH_INTERVAL_MS, true);
	}

	override void OnMissionFinish()
	{
		if (g_Game && g_Game.IsServer())
		{
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(CVF_WatchServerConfig);
			CVF_Logger.Log("Mission finishing. Generating next-boot vehicle config overrides.");
			GetCVFConfigManager().ReloadConfigForNewSpawns();
		}

		super.OnMissionFinish();
	}

	override void OnClientReadyEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientReadyEvent(identity, player);

		if (!g_Game || !g_Game.IsServer() || !identity)
			return;

		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CVF_SendSyncHelloToClient, 750, false, identity);
	}

	override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
	{
		CVF_NetworkSync.RemoveClient(identity);
		super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
	}
}

void CVF_LoadServerConfig()
{
	GetCVFConfigManager().LoadConfig();
	if (GetCVFConfigManager().HasLoadError())
	{
		CVF_Logger.Error("Framework disabled for this startup because vehicles.json could not be loaded.");
		return;
	}
	if (!GetCVFConfigManager().WasLastGenerationSuccessful())
	{
		CVF_Logger.Error("Framework disabled for this startup because the native override package could not be generated.");
		return;
	}

	if (GetCVFConfigManager().IsFrameworkEnabled())
	{
		CVF_Logger.Log("Framework enabled.");
		if (!CVF_ConfigGenerator.IsLoadedConfigCurrent())
			CVF_Logger.Warning("The generated native package is not active in this process. Another server restart is required.");
	}
	else
		CVF_Logger.Log("Framework disabled.");
}

void CVF_SendSyncHelloToClient(PlayerIdentity identity)
{
	CVF_NetworkSync.SendHelloToClient(identity);
}

void CVF_WatchServerConfig()
{
	GetCVFConfigManager().ReloadConfigIfChanged();
}
