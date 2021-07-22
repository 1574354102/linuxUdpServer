#include<iostream>
#include<unistd.h>
#include<map>
#include<string>
#include"HPSocket.h"
#include<cstring>
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
using namespace std;
using namespace rapidjson;

constexpr auto PrivateServerIP = "172.25.150.146";
constexpr auto ServerPort = 3080;
constexpr auto UserNameLen = 20;

struct UserInfo {
	string UserName;
	int UnConnectTimes;

	UserInfo(const string& UserName, const int& UnConnectTimes)
	{
		this->UserName = UserName;
		this->UnConnectTimes = UnConnectTimes;
	}
};

map<string, UserInfo> mapClient;

class CListenerImpl : public IUdpNodeListener
{
public:

	virtual EnHandleResult OnPrepareListen(IUdpNode* pSender, SOCKET soListen)
	{
		cout << "OnPrepareListen" << endl;
		return HR_OK;
	}
	virtual EnHandleResult OnReceive(IUdpNode* pSender, LPCTSTR lpszRemoteAddress, USHORT usRemotePort, const BYTE* pData, int iLength)
	{
		char clientInfo[30];
		sprintf(clientInfo, "%s_%d", lpszRemoteAddress, usRemotePort);

		auto sendUser = mapClient.find(string(clientInfo));
		Document dom;
		if (dom.Parse((char*)pData, iLength).HasParseError())
		{
			return HR_IGNORE;
		}

		if (dom.HasMember("msgType") && dom["msgType"].IsInt())
		{
			//收到消息
			if (!dom["msgType"].GetInt() && dom.HasMember("msgInfo") && dom["msgInfo"].IsString())
			{
				cout << "Receive message from " << lpszRemoteAddress << "::" << usRemotePort << endl;
				for (auto& i : mapClient)
				{
					string ipTmp(i.first), portTmp(i.first);
					int index = ipTmp.find('_');
					ipTmp.replace(index, i.first.size() - index, "");
					portTmp.replace(0, index + 1, "");

					StringBuffer buf;
					Writer<StringBuffer> writer(buf);

					writer.StartObject();

					writer.Key("msgType");
					if (i.first.compare(clientInfo))
					{
						writer.Int(1);		

						writer.Key("userName");
						writer.String(sendUser->second.UserName.c_str());
					}
					else
					{
						writer.Int(0);
					}

					writer.Key("msgInfo");
					writer.String(dom["msgInfo"].GetString());

					writer.EndObject();

					pSender->Send(ipTmp.c_str(), stoi(portTmp), (BYTE*)buf.GetString(), buf.GetSize());
				}
			}
			//收到心跳包
			else if (dom["msgType"].GetInt() == 1 && dom.HasMember("userName") && dom["userName"].IsString())
			{
				cout << "Receive HeartBeat Packets" << endl;
				mapClient.insert_or_assign(string(clientInfo), UserInfo(string(dom["userName"].GetString()), 0));
			}
		}
		return HR_OK;
	}
	virtual EnHandleResult OnSend(IUdpNode* pSender, LPCTSTR lpszRemoteAddress, USHORT usRemotePort, const BYTE* pData, int iLength)
	{
		cout << "Send to " << lpszRemoteAddress << "::" << usRemotePort << endl;
		cout << pData << " " << iLength << endl;

		return HR_OK;
	}
	virtual EnHandleResult OnError(IUdpNode* pSender, EnSocketOperation enOperation, int iErrorCode, LPCTSTR lpszRemoteAddress, USHORT usRemotePort, const BYTE* pData, int iLength)
	{
		cout << iErrorCode << endl;
		return HR_OK;
	}
	virtual EnHandleResult OnShutdown(IUdpNode* pSender)
	{
		cout << "OnShutdown" << endl;
		return HR_OK;
	}
};

int main()
{
	CListenerImpl listener;
	CUdpNodePtr pUdpNode(&listener);

	pUdpNode->Start(PrivateServerIP, ServerPort);

	while (1)
	{
		StringBuffer buf;
		Writer<StringBuffer> writer(buf);
		
		writer.StartObject();
		writer.Key("msgType");
		writer.Int(2);
		
		writer.Key("onlineUsersName");
		writer.StartArray();
		for (auto i = mapClient.begin(); i != mapClient.end(); )
		{
			if (i->second.UnConnectTimes >= 2)
			{
				i = mapClient.erase(i);
			}
			else
			{
				writer.String(i->second.UserName.c_str());				
				i->second.UnConnectTimes++;
				i++;
			}
		}
		writer.EndArray();
		writer.EndObject();
		for (auto i : mapClient)
		{
			string ipTmp(i.first), portTmp(i.first);
			int index = ipTmp.find('_');
			ipTmp.replace(index, i.first.size() - index, "");
			portTmp.replace(0, index + 1, "");

			pUdpNode->Send(ipTmp.c_str(), stoi(portTmp), (BYTE*)buf.GetString(), buf.GetSize());
		}
	
		sleep(1);
	}
	pUdpNode->Stop();
	return 0;
}