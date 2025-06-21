#ifndef GAME_CLIENT_COMPONENTS_SHEEP_UPDATE_H
#define GAME_CLIENT_COMPONENTS_SHEEP_UPDATE_H

#include <game/client/component.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>

class CUpdate : public CComponent
{
    static constexpr const char *m_VersionUrl = "https://raw.githubusercontent.com/ianzano/sheep-client/refs/heads/master/version.json";
    
public:
    int m_MinVersion = 0;
    int m_CurVersionInternal = 0;
    char m_CurVersion[16] = "unbekannt";

	std::shared_ptr<CHttpRequest> m_RequestTask = nullptr;
    
	void FetchClientInfo();
	void HandleClientInfo(json_value pJson);

	virtual void OnInit() override;
	virtual void OnRender() override;

    int Sizeof() const override { return sizeof(*this); }
};

#endif