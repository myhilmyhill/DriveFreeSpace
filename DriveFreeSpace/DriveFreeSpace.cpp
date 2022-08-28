/*
	TVTest プラグイン

	ドライブの空き容量
*/


#include <format>
#include <string>
#include <vector>
#include <windows.h>
#define TVTEST_PLUGIN_CLASS_IMPLEMENT
#include "TVTestPlugin.h"


// ステータス項目の識別子
constexpr auto STATUS_ITEM_ID = 1;
constexpr auto pszPluginName = L"ドライブの空き容量";

// プラグインクラス
class CDriveFreeSpace : public TVTest::CTVTestPlugin
{
	size_t m_SelectedDriveIndex = 0;
	std::vector<std::wstring> m_Drives;

	static LRESULT CALLBACK EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2, void* pClientData);
	bool GetDrives();
	void ShowItem(bool fShow);

public:
	CDriveFreeSpace()
	{
	}

	bool GetPluginInfo(TVTest::PluginInfo* pInfo) override;
	bool Initialize() override;
	bool Finalize() override;
};


bool CDriveFreeSpace::GetPluginInfo(TVTest::PluginInfo* pInfo)
{
	// プラグインの情報を返す
	pInfo->Type = TVTest::PLUGIN_TYPE_NORMAL;
	pInfo->Flags = 0;
	pInfo->pszPluginName = pszPluginName;
	pInfo->pszCopyright = L"Public Domain";
	pInfo->pszDescription = L"ドライブの空き容量をステータスバーに表示します。左クリックで表示するドライブを切り替えられます。";
	return true;
}


bool CDriveFreeSpace::Initialize()
{
	// 初期化処理

	// ステータス項目を登録
	TVTest::StatusItemInfo Item;
	Item.Size = sizeof(Item);
	Item.Flags = TVTest::STATUS_ITEM_FLAG_TIMERUPDATE;
	Item.Style = 0;
	Item.ID = STATUS_ITEM_ID;
	Item.pszIDText = L"DriveFreeSpace";
	Item.pszName = pszPluginName;
	Item.MinWidth = 0;
	Item.MaxWidth = -1;
	Item.DefaultWidth = TVTest::StatusItemWidthByFontSize(8);
	Item.MinHeight = 0;
	if (!m_pApp->RegisterStatusItem(&Item)) {
		m_pApp->AddLog(L"ステータス項目を登録できません。", TVTest::LOG_TYPE_ERROR);
		return false;
	}

	// イベントコールバック関数を登録
	m_pApp->SetEventCallback(EventCallback, this);

	return true;
}


bool CDriveFreeSpace::GetDrives()
{
	WCHAR szDrives[MAX_PATH] = { 0 };
	const auto dwResult = ::GetLogicalDriveStringsW(sizeof(szDrives) - 1, szDrives);
	m_Drives = std::vector<std::wstring>();

	if (!(dwResult > 0 && dwResult <= sizeof(szDrives)))
		return false;

	WCHAR* szDrive = szDrives;
	while (*szDrive)
	{
		m_Drives.push_back(szDrive);

		// get the next drive
		szDrive += lstrlenW(szDrive) + 1;
	}

	return true;
}


bool CDriveFreeSpace::Finalize()
{
	// 終了処理

	return true;
}


// イベントコールバック関数
// 何かイベントが起きると呼ばれる
LRESULT CALLBACK CDriveFreeSpace::EventCallback(
	UINT Event, LPARAM lParam1, LPARAM lParam2, void* pClientData)
{
	CDriveFreeSpace* pThis = static_cast<CDriveFreeSpace*>(pClientData);

	switch (Event) {
	case TVTest::EVENT_PLUGINENABLE:
		// プラグインの有効状態が変化した
		// プラグインの有効状態と項目の表示状態を同期する
	{
		const bool show = lParam1 != 0;
		pThis->ShowItem(show);
		if (show) {
			return pThis->GetDrives();
		}
		else {
			return TRUE;
		}
	}

	case TVTest::EVENT_STATUSITEM_DRAW:
		// ステータス項目の描画
	{
		const TVTest::StatusItemDrawInfo* pInfo =
			reinterpret_cast<const TVTest::StatusItemDrawInfo*>(lParam1);

		if ((pInfo->Flags & TVTest::STATUS_ITEM_DRAW_FLAG_PREVIEW) == 0) {
			// 通常の項目の描画

			auto drive = pThis->m_Drives.at(pThis->m_SelectedDriveIndex).c_str();
			ULARGE_INTEGER FreeSpace;

			std::wstring freeSpaceStr;
			if (::GetDiskFreeSpaceExW(drive, &FreeSpace, nullptr, nullptr)) {
				const auto freeSpaceGb = FreeSpace.QuadPart / 1024.0 / 1024 / 1024;
				freeSpaceStr = std::format(L"{} {:.2f} GB", drive, freeSpaceGb);
			}
			else {
				freeSpaceStr = std::format(L"{} unavailable", drive);
			}

			pThis->m_pApp->ThemeDrawText(
				pInfo->pszStyle, pInfo->hdc, freeSpaceStr.c_str(),
				pInfo->DrawRect,
				DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS,
				pInfo->Color);
			return TRUE;
		}
		else {
			// プレビュー(設定ダイアログ)の項目の描画
			auto szText = L"C:\\ 1000.00 GB";
			pThis->m_pApp->ThemeDrawText(
				pInfo->pszStyle, pInfo->hdc, szText,
				pInfo->DrawRect,
				DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS,
				pInfo->Color);
		}
	}
	return TRUE;

	case TVTest::EVENT_STATUSITEM_NOTIFY:
		// ステータス項目の通知
	{
		const TVTest::StatusItemEventInfo* pInfo =
			reinterpret_cast<const TVTest::StatusItemEventInfo*>(lParam1);

		switch (pInfo->Event) {
		case TVTest::STATUS_ITEM_EVENT_CREATED:
			// 項目が作成された
			// プラグインが有効であれば項目を表示状態にする
			pThis->ShowItem(pThis->m_pApp->IsPluginEnabled());
			return TRUE;

		case TVTest::STATUS_ITEM_EVENT_VISIBILITYCHANGED:
			// 項目の表示状態が変わった
			// 項目の表示状態とプラグインの有効状態を同期する
			pThis->m_pApp->EnablePlugin(pInfo->Param != 0);
			return TRUE;

		case TVTest::STATUS_ITEM_EVENT_UPDATETIMER:
			// 更新タイマー
			return TRUE; // TRUE を返すと再描画される
		}
	}
	return FALSE;

	case TVTest::EVENT_STATUSITEM_MOUSE:
		// ステータス項目のマウス操作
	{
		const TVTest::StatusItemMouseEventInfo* pInfo =
			reinterpret_cast<const TVTest::StatusItemMouseEventInfo*>(lParam1);

		// 左ボタンが押されたら次のドライブを表示
		if (pInfo->Action == TVTest::STATUS_ITEM_MOUSE_ACTION_LDOWN) {
			const size_t selectedDriveIndex = (pThis->m_SelectedDriveIndex + 1) % pThis->m_Drives.size();
			::InterlockedExchange(&pThis->m_SelectedDriveIndex, selectedDriveIndex);

			return TRUE;
		}
	}
	return FALSE;
	}

	return 0;
}


// ステータス項目の表示/非表示を切り替える
void CDriveFreeSpace::ShowItem(bool fShow)
{
	TVTest::StatusItemSetInfo Info;

	Info.Size = sizeof(Info);
	Info.Mask = TVTest::STATUS_ITEM_SET_INFO_MASK_STATE;
	Info.ID = STATUS_ITEM_ID;
	Info.StateMask = TVTest::STATUS_ITEM_STATE_VISIBLE;
	Info.State = fShow ? TVTest::STATUS_ITEM_STATE_VISIBLE : 0;

	m_pApp->SetStatusItem(&Info);
}


TVTest::CTVTestPlugin* CreatePluginClass()
{
	return new CDriveFreeSpace;
}
