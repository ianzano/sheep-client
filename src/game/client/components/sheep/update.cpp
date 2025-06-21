#include <engine/shared/json.h>

#include <base/log.h>

#include "update.h"

void CUpdate::OnInit()
{
	FetchClientInfo();
}

void CUpdate::FetchClientInfo()
{
	if(m_RequestTask && !m_RequestTask->Done()) {
		return;
	}
	
	m_RequestTask = HttpGet(m_VersionUrl);
	m_RequestTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_RequestTask->IpResolve(IPRESOLVE::V4);
	Http()->Run(m_RequestTask);
}

void CUpdate::OnRender()
{
	if(m_RequestTask) {
		if (m_RequestTask->State() == EHttpState::DONE) {
			json_value *pJson = m_RequestTask->ResultJson();
			HandleClientInfo(*pJson);
			json_value_free(pJson);
		}

		if (m_RequestTask->State() != EHttpState::RUNNING && m_RequestTask->State() != EHttpState::QUEUED) {
			log_info("update", "Checked version");
			m_RequestTask->Abort();
			m_RequestTask = NULL;
		}
	}
}

void CUpdate::HandleClientInfo(json_value &pJson)
{
	m_MinVersion = pJson["minVersion"].type == json_integer ? json_int_get(&pJson["minVersion"]) : 0;
	if (pJson["curVersion"].type == json_string) {
		str_copy(m_CurVersion, pJson["curVersion"]);
	}
	m_CurVersionInternal = pJson["m_CurVersionInternal"].type == json_integer ? json_int_get(&pJson["m_CurVersionInternal"]) : 0;
	log_info("update", "Current version: %s, Min version: %d, Internal version: %d", m_CurVersion, m_MinVersion, m_CurVersionInternal);
}