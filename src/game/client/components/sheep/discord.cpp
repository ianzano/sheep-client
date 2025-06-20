#include "../../gameclient.h"

#include "discord.h"
#include <base/log.h>

void CSDiscord::OnInit()
{
	m_DiscordBot = new dpp::cluster(m_Token, dpp::i_default_intents | dpp::i_message_content);
	m_DiscordBot->start(dpp::st_return);

	m_DiscordBot->on_message_create([this](const dpp::message_create_t &event) {
		std::string channelName;

		if(event.msg.author.id == m_DiscordBot->me.id)
		{
			return;
		}

		if(event.msg.channel_id == m_Channel->id)
		{
			channelName = "Server";
		}

		if(channelName.empty())
		{
			for(auto const &[name, id] : m_Channels)
			{
				if(event.msg.channel_id == id)
				{
					channelName = name;
					break;
				}
			}
		}

		if(channelName.empty())
		{
			return;
		}

		GameClient()->m_Chat.AddLine(DISCORD_MSG, 0, ("(" + channelName + ") " + event.msg.author.global_name + ": " + event.msg.content.c_str()).c_str());
	});
}

bool CSDiscord::IsPlayerOnline(const std::string player)
{
	// check if the player is online
	for(const CNetObj_PlayerInfo *pInfo : GameClient()->m_Snap.m_apInfoByName)
	{
		if(!pInfo)
		{
			continue;
		}

		if(strcmp(GameClient()->m_aClients[pInfo->m_ClientId].m_aName, player.c_str()) == 0)
		{
			return true;
		}
	}

	return false;
}

void CSDiscord::OnMapLoad()
{
	if(!m_DiscordBot)
	{
		return;
	}

	if(GameClient()->m_Update.m_MinVersion > GameClient()->m_Update.m_InternalVersion)
	{
		log_error("discord", "Minimum version for client not met. Need: %d / Is: %d.", GameClient()->m_Update.m_MinVersion, GameClient()->m_Update.m_InternalVersion);
		return;
	}

	m_DiscordBot->channels_get(m_GuildId, [this](const dpp::confirmation_callback_t &event) {
		if(event.is_error())
		{
			log_error("discord", "Error fetching all discord channels. Reason: %s", event.get_error().message.c_str());
			return;
		}

		dpp::channel_map map = std::get<dpp::channel_map>(event.value);

		for(const std::pair<const dpp::snowflake, dpp::channel> &pair : map)
		{
			dpp::channel channel = pair.second;

			// TODO: read channel names from here and put them into m_Channels instead of hard-coding

			if(channel.parent_id != m_CategoryId)
			{
				continue;
			}

			if(channel.name == GenerateChannelName())
			{
				m_Channel = new dpp::channel(channel);

				if(channel.topic == Client()->PlayerName())
				{
					m_SendMessages = true;
				}
				else if(!IsPlayerOnline(channel.topic))
				{
					channel.set_topic(std::string(Client()->PlayerName()));
					m_DiscordBot->channel_edit(channel, [this, &channel](const dpp::confirmation_callback_t &event) {
						if(!event.is_error())
						{
							m_SendMessages = true;
							log_info("discord", "Updated channel %s topic to: %s", channel.name.c_str(), Client()->PlayerName());
						}
						else
						{
							log_info("discord", "Failed to update channel %s topic to: %s. Reason: %s", channel.name.c_str(), Client()->PlayerName(), event.get_error().message.c_str());
						}
					});
				}

				return;
			}
		}

		if(!m_Channel)
		{
			CreateChannel();
		}
	});
}

std::string CSDiscord::GenerateChannelName()
{
	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);
	std::string output = std::format("{}-{}", CurrentServerInfo.m_aName, CurrentServerInfo.m_aAddress);
	return Normalize(output);
}

std::string CSDiscord::Normalize(std::string &str)
{
	std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
	str = ReplaceAll(str, " ", "");
	str = ReplaceAll(str, ":", "");
	str = ReplaceAll(str, ".", "");
	return str;
}

std::string CSDiscord::ReplaceAll(std::string &str, const std::string &from, const std::string &to)
{
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
	return str;
}

void CSDiscord::CreateChannel()
{
	if(!m_DiscordBot)
	{
		return;
	}

	dpp::channel channel;
	channel.set_name(GenerateChannelName());
	channel.set_type(dpp::channel_type::CHANNEL_TEXT);
	channel.set_topic(Client()->PlayerName());
	channel.set_parent_id(m_CategoryId);
	channel.set_guild_id(m_GuildId);

	m_DiscordBot->channel_create(channel, [this](const dpp::confirmation_callback_t &event) {
		if(!event.is_error())
		{
			m_Channel = new dpp::channel(std::get<dpp::channel>(event.value));
			m_SendMessages = true;
			log_info("discord", "Successfully created channel %s", GenerateChannelName().c_str());
		}
		else
		{
			log_error("discord", "Failed to create channel %s. Reason: %s", GenerateChannelName().c_str(), event.get_error().message);
		}
	});
}

void CSDiscord::OnMessage(int Msg, void *pRawMsg)
{
	if(!m_DiscordBot || !m_SendMessages || !m_Channel)
	{
		return;
	}

	if(Msg != NETMSGTYPE_SV_CHAT)
	{
		return;
	}

	CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

	if(
		// pMsg->m_ClientId == SERVER_MSG ||
		pMsg->m_ClientId == CLIENT_MSG ||
		pMsg->m_Team == TEAM_WHISPER_SEND ||
		pMsg->m_Team == TEAM_WHISPER_RECV)
	{
		return;
	}

	char aBuf[1024];
	if(pMsg->m_ClientId >= 0)
	{
		// bool team = pMsg->m_Team == 1;
		// bool whisper = pMsg->m_Team >= 2;
		const auto author = m_pClient->m_aClients[pMsg->m_ClientId];
		int color = TEAM_ALL;

		if(author.m_Active)
		{
			if(author.m_Team == TEAM_SPECTATORS)
			{
				color = TEAM_SPECTATORS;
			}

			if(m_pClient->IsTeamPlay())
			{
				if(author.m_Team == TEAM_RED)
				{
					color = TEAM_RED;
				}
				else if(author.m_Team == TEAM_BLUE)
				{
					color = TEAM_BLUE;
				}
			}
		}

		if(pMsg->m_Team == 1)
		{
			str_format(aBuf, sizeof(aBuf), "__**%s >>> **%s__", author.m_aName, pMsg->m_pMessage);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "**%s > **%s", author.m_aName, pMsg->m_pMessage);
		}
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "__*%s*__", pMsg->m_pMessage);
	}

	m_DiscordBot->message_create(dpp::message(m_Channel->id, aBuf), [this](const dpp::confirmation_callback_t &event) {
		if(event.is_error())
		{
			log_error("discord", "Failed to transmit message. Reason: %s", event.get_error().message.c_str());
		}
	});
}

void CSDiscord::UpdateName()
{
	if(!m_DiscordBot || !m_SendMessages || !m_Channel)
	{
		return;
	}

	m_Channel->set_topic(Client()->PlayerName());
	m_DiscordBot->channel_edit(*m_Channel, [this](const dpp::confirmation_callback_t &event) {
		if(!event.is_error())
		{
			log_info("discord", "Channel topic updated to: %s", Client()->PlayerName());
		}
		else
		{
			log_error("discord", "Failed to update channel topic. Reason: %s", event.get_error().message.c_str());
		}
	});
}

void CSDiscord::LeaveChannel()
{
	if(!m_Channel)
	{
		return;
	}

	m_Channel->set_topic(" ");
	m_DiscordBot->channel_edit(*m_Channel, [this](const dpp::confirmation_callback_t &event) {
		if(!event.is_error())
		{
			log_info("discord", "Channel topic cleared.");
		}
		else
		{
			log_error("discord", "Failed to clear channel topic. Reason: %s", event.get_error().message.c_str());
		}
	});
	m_SendMessages = false;
}

void CSDiscord::OnReset()
{
	LeaveChannel();
}

void CSDiscord::OnShutdown()
{
	LeaveChannel();

	if(m_DiscordBot)
	{
		m_DiscordBot->shutdown();
	}
}