#ifndef GAME_CLIENT_COMPONENTS_SHEEP_DISCORD_H
#define GAME_CLIENT_COMPONENTS_SHEEP_DISCORD_H

#include <game/client/component.h>

#undef log_error
#include <dpp/dpp.h>
#undef ERROR
#undef DELETE
#undef IMAGE_CURSOR
#define log_error(sys, ...) log_log(LEVEL_ERROR, sys, __VA_ARGS__)

class CSDiscord : public CComponent
{
public:
	const char *m_ReleaseVersion = "1.00.0000";
	const int m_InternalVersion = 1000000;

	CSDiscord();

	void OnMapLoad() override;
	void OnShutdown() override;
	void OnReset() override;
	void OnMessage(int Msg, void *pRawMsg) override;

	void UpdateName();

	void LeaveChannel();
	void CreateChannel();
	std::string GenerateChannelName();

	int Sizeof() const override { return sizeof(*this); }

private:
	std::string m_Token = "";
	dpp::snowflake m_GuildId = dpp::snowflake("1377380381908533439");
	dpp::snowflake m_CategoryId = dpp::snowflake("1383359347094192148");
	dpp::snowflake m_ChannelId = dpp::snowflake("1383527961239752837");

	std::map<std::string, dpp::snowflake> m_Channels = {
		{"just-talk", dpp::snowflake("1377382970406473831")}};

	dpp::cluster *m_DiscordBot;
	dpp::channel *m_Channel;

	std::string Normalize(std::string &str);
	std::string ReplaceAll(std::string &str, const std::string &from, const std::string &to);

	bool m_SendMessages = false;

	bool IsPlayerOnline(std::string player);

	// ugly! see ../chat.h -> but i wanna touch as less existing code as possible
	enum
	{
		DISCORD_MSG = -3,
		CLIENT_MSG = -2,
		SERVER_MSG = -1,
	};
};

#endif