class CfgPatches
{
    class CustomVehicleFramework
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Vehicles_Wheeled"};
    };
};

class CfgCustomVehicleFramework
{
    class GeneratedOverrideProbe
    {
        cvfProtocol = 0;
        cvfPayloadHash = 0;
        cvfPayloadChars = 0;
        cvfServerId = "";
    };
};

class CfgMods
{
    class CustomVehicleFramework
    {
        dir = "CustomVehicleFramework";
        picture = "";
        action = "";
        hideName = 1;
        hidePicture = 1;
        name = "Custom Vehicle Framework";
        credits = "Blubber";
        author = "Blubber";
        authorID = "76561197995145122";
        version = "1.5";
        extra = 0;
        type = "mod";

        dependencies[] = {"Core", "Game", "World", "Mission"};

        class defs
        {
            class engineScriptModule
            {
                value = "";
                files[] = {"CustomVehicleFramework/Scripts/1_Core"};
            };
            class gameScriptModule
            {
                value = "";
                files[] = {"CustomVehicleFramework/Scripts/3_Game"};
            };
            class worldScriptModule
            {
                value = "";
                files[] = {"CustomVehicleFramework/Scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"CustomVehicleFramework/Scripts/5_Mission"};
            };
        };
    };
};
