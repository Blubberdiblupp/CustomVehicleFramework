class CVF_ClientUI
{
	private static string s_PendingTitle = "";
	private static string s_PendingMessage = "";
	private static int s_DialogAttempts = 0;

	static void ShowError(string message)
	{
		CVF_Logger.Error(message);
		if (NotificationSystem.GetInstance())
			NotificationSystem.AddNotificationExtended(30.0, "Custom Vehicle Framework Error", message);
		else
			QueueMainMenuDialog("Custom Vehicle Framework Error", message);
	}

	private static void QueueMainMenuDialog(string title, string message)
	{
		s_PendingTitle = title;
		s_PendingMessage = message;
		s_DialogAttempts = 0;
		if (g_Game)
			g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(ShowPendingDialog, 750, false);
	}

	private static void ShowPendingDialog()
	{
		if (s_PendingMessage == "")
			return;
		if (!g_Game || !g_Game.GetUIManager())
		{
			s_DialogAttempts++;
			if (g_Game && s_DialogAttempts < 10)
				g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(ShowPendingDialog, 500, false);
			return;
		}

		g_Game.GetUIManager().ShowDialog(s_PendingTitle, s_PendingMessage, 44066, DBT_OK, DBB_NONE, DMT_INFO, g_Game.GetUIManager().GetMenu());
		s_PendingTitle = "";
		s_PendingMessage = "";
		s_DialogAttempts = 0;
	}
}
