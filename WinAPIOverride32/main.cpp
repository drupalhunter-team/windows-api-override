/*
Copyright (C) 2004 Jacquelin POTIER <jacquelin.potier@free.fr>
Dynamic aspect ratio code Copyright (C) 2004 Jacquelin POTIER <jacquelin.potier@free.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "resource.h"
#include "StructsAndDefines.h"
#include "about.h"
#include "filters.h"
#include "modulesfilters.h"
#include "ResizeMainDialog.h"
#include "SelectColumns.h"
#include "selecthookedprocess.h"
#include "OpenSave.h"
#include "monitoringwizard.h"
#include "CompareLogs.h"
#include "Stats.h"
#include "Search.h"
#include "RemoteCall.h"
#include "StringBytesConvert.h"
#include "memoryuserinterface.h"
#include "HexDisplay.h"
#include "OptionsUserInterface.h"
#include "ComClsidNameConvert.h"
#include "ComIidNameConvert.h"
#include "CommandLineParsing.h"
#include "comcomponents.h"

#include "WinApiOverride.h"
#include "PluginManager.h"
#include "../tools/Process/ProcessName/ProcessName.h"
#include "../tools/string/AnsiUnicodeConvert.h"
#include "../tools/Process/APIOverride/HookCom/HookComExport.h"
#include "../tools/Process/ProcessHelper/ProcessHelper.h"
#include "../tools/GUI/SelectWindow/selectwindow.h"
#include "../tools/GUI/ListBox/Listbox.h"
#include "../tools/GUI/SysLink/SysLink.h"
#include "../Tools/GUI/Dialog/DialogHelper.h"
#include "../Tools/GUI/ToolTip/ToolTip.h"
#include "../Tools/GUI/Rebar/Rebar.h"
#include "../Tools/Process/ProcessMonitor/ProcMonInterface.h"
#include "../Tools/LinkList/LinkList.h"
#include "../Tools/LinkList/LinkListSimple.h"
#include "../Tools/PE/PE.h"
#include "../Tools/Gui/ToolBar/Toolbar.h"
#include "../Tools/Gui/Splitter/Splitter.h"
#include "../Tools/File/StdFileOperations.h"
#include "../Tools/Zip/XZip.h"
#include "../Tools/Zip/XUnzip.h"
#include "../tools/Privilege/privilege.h"
#include "../tools/softwareupdate/softwareupdate.h"
#include "../Tools/DebugInfos/DebugInfosClasses/DebugInfos.h"
#include "../Tools/ShellChangeNotifications/IShellChangeNotifications.h"
#include "../Tools/String/StringConverter.h"

#pragma intrinsic (memcpy,memset,memcmp)

/////////////////////////////////////////
// defines
/////////////////////////////////////////

// defines for update checking
#define WinApioverride_PAD_Url _T("http://jacquelin.potier.free.fr/winapioverride32/WinAPIOverride.xml")
#define WinApioverrideVersionSignificantDigits 3
#if (defined(UNICODE)||defined(_UNICODE))
    #define WinApioverrideDownloadLink _T("http://jacquelin.potier.free.fr/exe/winapioverride32_bin.zip")
#else
    #define WinApioverrideDownloadLink _T("http://jacquelin.potier.free.fr/exe/winapioverride32_bin_ansi.zip")
#endif

// hook com dll name (used for IDispatch parsing of COM components)
#define HOOKCOM_DLL_NAME _T("HookCom.dll")

#ifndef _countof
    #define _countof(array) (sizeof(array)/sizeof(array[0]))
#endif

#define WM_USER_CROSS_THREAD_SET_FOCUS (WM_USER+1)

/////////////////////////////////////////
// vars
/////////////////////////////////////////
CLinkListSimple* pApiOverrideList=NULL; // linked list containing all used CAPIOverride objects. Contains pointer of CApiOverride* (to allow to remove deleted CApiOverride objects from list)
CLinkList* pLogList=NULL;// linked list of logs (list of LOG_LIST_ENTRY)
HANDLE mhLogHeap=NULL;
CSelectWindow* pSelectWindow=NULL;// Class to graphically select an other window and retrieve information about selected window
CListview* pListview=NULL;// log listview
CListview* pListviewDetails=NULL;// details listview
CListview* pListviewDetailsTypesDisplay=NULL;
LOG_ENTRY* pLogListviewDetails=NULL;// current log displayed in detail listview
CProcMonInterface* pProcMonInterface=NULL;// class helper for using ProcMon driver (driver used for signaling process creation and ends)
CFilters* pFilters=NULL;// class for the filters dialog
CListview* pListViewMonitoringFiles=NULL;// listview containing monitoring files
CListview* pListViewOverridingFiles=NULL;// listview containing overriding dll
CRebar* pRebar=NULL;
CToolbar* pToolbar=NULL;// toolbar object
CPopUpMenu* pExportPopUpMenu=NULL;// export pop up menu object
CPopUpMenu* pCOMToolsPopUpMenu=NULL;// Com Tools pop up menu
CPopUpMenu* pErrorMessagePopUpMenu=NULL;// Error message pop up menu
CPopUpMenu* pPluginsPopUpMenu=NULL;// Plugin pop up menu
DWORD dwLogId=0;// Previous Log Id
CLinkList* pAnalysisInfoMessages=NULL;// analysis information messages (used to color them easily)
CPluginManager* pPluginManager=NULL;// object that manages plugins informations

// Menu Id of Export menu
UINT idMenuExportHTML=0;
UINT idMenuExportXML=0;
UINT idMenuExportTXT=0;
UINT idMenuExportCSV=0;
UINT idMenuExportOnlySelected=0;
UINT idMenuExportOnlyVisible=0;
UINT idMenuExportFullParametersContent=0;

// Menu Id of error message menu
UINT idMenuErrorMessageGetLastError =0;
UINT idMenuErrorMessageReturnedValue=0;

// menu Id of Com tools menu
UINT idMenuComToolsHookedComObjectInteraction=0;
UINT idMenuComToolsNameCLSIDConvert=0;
UINT idMenuComToolsNameIIDConvert=0;
UINT idMenuComToolsShowMethodsAddressInModule=0;
UINT idMenuComToolsMonitoringInterfaceWizard=0;
UINT idMenuComToolsShowAllCLSID=0;

CSplitter* pSplitterLoadMonitoringAndFaking=NULL;// Splitter object
CSplitter* pSplitterDetails=NULL;// Splitter object
CSplitter* pSplitterConfig=NULL;// Splitter object
COptions* pOptions=NULL;//options object

// Tooltips
CToolTip* pToolTipEditModulesExclusionFilters=NULL;
CToolTip* pToolTipReloadModulesExclusionFilters=NULL;
CToolTip* pToolTipMonitoringWizard=NULL;
CToolTip* pToolTipMonitoringReloadLastSession=NULL;
CToolTip* pToolTipFakingReloadLastSession=NULL;


HINSTANCE mhInstance=NULL;// app instance
HWND mhWndDialog=NULL;// main dialog handle
BOOL bMonitoring=TRUE;// flag to know if APIoverride is monitoring
BOOL bFaking=TRUE;// flag to know if APIoverride is faking
BOOL bStarted=FALSE;// flag to know if APIoverride is started
BOOL bExitingStop=FALSE;// flag to know user has asked to stop application and we are currently stopping APIoverride
BOOL bExit=FALSE;// flag to know that app has asked to exit
BOOL bIDC_EDIT_PIDChangedInternally=FALSE;// flag to known if IDC_EDIT_PID is changed by user or internally by winapioverride
BOOL bUserInterfaceInStartedMode=FALSE;// flag to know if UserInterface is in started or stopped design
HANDLE evtUpdateListView=NULL;// query the listview to be updated
HANDLE evtApplicationExit=NULL;// end query
int LastSelectedItemIndexInMainView=-1;// last selected item in main listview
HANDLE evtStopping=NULL;// signaled if in stopping operation
HANDLE hevtUnexpectedUnload=NULL;
HANDLE evtNoGuiTargetStoppedByStopAndKillAfter=NULL;
HANDLE hDetailListviewUpdaterThread = NULL;
HMODULE hHookComDll=NULL;
pfHookComShowMethodsAddress pHookComShowMethodsAddress;
TCHAR pszApplicationPath[MAX_PATH];// winapioverride path
BOOL bLoadCommandLineMonitoringAndFakingFilesNeverCalled=TRUE;

DWORD WaitCursorCount=0;
HCURSOR hWaitCursor=NULL;
HICON hIconWizard=NULL;
HICON hIconRefresh=NULL;
HICON hIconEdit=NULL;
HHOOK hKeyboardHook;

int DetailListViewMinArg=-1;
int DetailListViewMaxArg=-1;
int DetailListViewMinStack=-1;
int DetailListViewMaxStack=-1;
int DetailListViewReturnValueIndex = -1;
BOOL bRemovingSelectedLogs = FALSE;
BOOL RemovingAllLogsEntries=FALSE;
int DetailListViewLastErrorIndex = -1;
UINT ListviewDetailsMenuIDHexDisplay=(UINT)-1;
HANDLE evtDetailListViewUpdateUnlocked=NULL;
HANDLE evtStartStopUnlocked=NULL;
HANDLE hThreadGetRemoteWindowInfos=NULL;
HANDLE hThreadUnloadMonitoringFile=NULL;
HANDLE hThreadUnloadOverridingFile=NULL;
HANDLE hThreadLoadLogs=NULL;
HANDLE hThreadSaveLogs=NULL;
HANDLE hThreadRemoveAllLogsEntries=NULL;
HANDLE hThreadRemoveSelectedLogs=NULL;
HANDLE hThreadCallAnalysis=NULL;
HANDLE hThreadReloadMonitoring=NULL;
HANDLE hThreadReloadOverriding=NULL;

COLORREF ListViewColorInformation=RGB(0,128,0);// listview item color of information reports
COLORREF ListViewColorError=RGB(255,0,0);// listview item color of error reports
COLORREF ListViewColorWarning=RGB(250,190,16);// listview item color of warning reports
COLORREF ListViewColorMessageBackGround=RGB(240,247,255);// listview item background color of reports
COLORREF ListViewColorFailureBackGround=RGB(255,174,174);// listview item background color for failure call
COLORREF ListViewColorDefaultBackGround=RGB(247,247,247);// listview item background color of 1/2 items
//COLORREF ListViewColorDefaultBackGround=RGB(241,242,243);// listview item background color of 1/2 items

// command lines var
BOOL bNoGUI=FALSE;// TRUE if WinApiOverride is asked to run without gui
BOOL bCommandLine=FALSE; // TRUE if application launched by command line
DWORD CommandLineMonitoringFilesArraySize=0;
TCHAR** CommandLineMonitoringFilesArray=NULL;
DWORD CommandLineOverridingDllArraySize=0;
TCHAR** CommandLineOverridingDllArray=NULL;
TCHAR* CommandLineNoGuiSavingFileName = NULL;

void ListviewDetailsSelectItemCallback(int ItemIndex,int SubItemIndex,LPVOID UserParam);
void ListviewDetailsPopUpMenuItemClickCallback(UINT MenuID,LPVOID UserParam);

typedef struct tagStopAfterTimerInfos
{
    HANDLE hProcess;
    DWORD StopTimerValue;
}STOP_AFTER_TIMER_INFOS;
DWORD WINAPI StopHookingAfterTimer(LPVOID lpParameter);

// User defines type change watcher
class CShellWatcher :public IShellItemChangeWatcher
{
    virtual void OnChangeNotify(long const lEvent, TCHAR* const sz1, TCHAR* const sz2)
    {
#ifdef _DEBUG
        TCHAR Msg[3*MAX_PATH];
        _stprintf(Msg,_T("Event : %8X, sz1 : %s, sz2 : %s\r\n"),lEvent,sz1,sz2);
        OutputDebugString(Msg);
#endif
        // Refresh current CSupportedParameters UserType cache 
        CSupportedParameters::ClearUserDataTypeCache();

        // Send Refresh command to Apioverride dll for cache updating too
        // next apioverride.dll will translate this refresh command to HookCom dll and Hooknet dll
        CLinkListItem* pItem;
        CApiOverride* pApiOverride;
        for ( pItem = pApiOverrideList->Head ; pItem ; pItem = pItem->NextItem)
        {
            pApiOverride = (CApiOverride*)pItem->ItemData;
            pApiOverride->ClearUserDataTypeCache();
        }
    }
};
CShellWatcher ShellWatcher;// global object definition
#define WM_SHELL_CHANGE_NOTIFY (WM_APP+200)


FORCEINLINE BOOL IsStopping()
{
    return (::WaitForSingleObject(evtStopping,0) == WAIT_OBJECT_0);
}

FORCEINLINE BOOL IsExiting()
{
    return (::WaitForSingleObject(evtApplicationExit,0) == WAIT_OBJECT_0);
}

void CleanThreadHandleIfNotExiting(HANDLE* phThread)
{
    if (!IsExiting())
    {
        ::CloseHandle(*phThread);
        *phThread = 0;
    }
}

void AssumeThreadEnd(HANDLE* phThread,SIZE_T TimeOut)
{
    HANDLE hThread = *phThread;

    if (hThread)
    {
        if (::WaitForSingleObject(hThread,TimeOut)==WAIT_TIMEOUT)
        {
            ::TerminateThread(hThread,0xDEADBEEF);
        }
        ::CloseHandle(hThread);
        *phThread = 0;
    }
}

//-----------------------------------------------------------------------------
// Name: WinMain
// Object: Entry point of app
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // create a special heap for logs
    mhLogHeap=HeapCreate(0,0x100000,0);

    // store application instance
    mhInstance=hInstance;

    // enable Xp style
    InitCommonControls();

    // if there's a command line
    if (*lpCmdLine!=0)
    {
        pOptions=new COptions();

        // create a CFilters object (must be created before calling ParseCommandLine)
        pFilters=new CFilters(pOptions);

        // parse command line
        if (!ParseCommandLine())
        {
            // on error exit
            FreeMemoryAllocatedByCommandLineParsing();
            delete pOptions;
            delete pFilters;
            HeapDestroy(mhLogHeap);
            return -1;
        }
    }
    else
    {
        // get current application directory
        CStdFileOperations::GetAppPath(pszApplicationPath,MAX_PATH);

        // make config file name
        TCHAR ConfigFileName[MAX_PATH];
        _tcscpy(ConfigFileName,pszApplicationPath);
        _tcscat(ConfigFileName,COPTION_OPTION_FILENAME);

        // load options
        pOptions=new COptions(ConfigFileName);
        // if configuration file doesn't exists
        if (!pOptions->Load())
        {
// do first launch special operations
        }

        pFilters=new CFilters(pOptions);
    }

    // start .Net profiling if needed
    if (pOptions->bNetProfilingEnabled)
    {
        CApiOverride::EnableNETProfilingStatic(TRUE);
    }

    // create a list for CApiOverride objects
    pApiOverrideList=new CLinkListSimple();

    // create a list for log entries
    pLogList=new CLinkList(sizeof(LOG_LIST_ENTRY));
    pLogList->SetHeap(mhLogHeap);

    // create ProcMon.sys driver interface
    pProcMonInterface=new CProcMonInterface();

    // create plugin manager object
    pPluginManager = new CPluginManager();

    // try to adjust privilege
    CPrivilege Privilege(FALSE);
    if (!Privilege.SetPrivilege(SE_DEBUG_NAME,TRUE))
    {
        if (!bNoGUI)
        {
            MessageBox(NULL,
                _T("Debug privileges can't be set, so you won't be able to hook other users processes"),
                _T("Information"),
                MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
        }
    }

    
    evtStopping=CreateEvent(NULL,TRUE,FALSE,NULL);// unsignaled manual reset
    evtStartStopUnlocked=CreateEvent(NULL,FALSE,TRUE,NULL);// signaled auto reset
    hevtUnexpectedUnload=CreateEvent(NULL,TRUE,FALSE,NULL); // unsignaled manual reset event
    // create a manual reset event with initial state to FALSE
    evtApplicationExit=CreateEvent(NULL,TRUE,FALSE,NULL); // should be created even in case of bNoGUI because it is used in StopHookingAfterTimer function

    // if no gui mode
    if (bNoGUI)
    {
        HANDLE WaitArray[2];
        SIZE_T WaitArraySize;
        WaitArray[0] = hevtUnexpectedUnload;
        WaitArraySize = 1;

        // in case of StopAndKillAfter, we have to watch another event as hevtUnexpectedUnload wont be raised due
        // to a clean hooking stop
        if (pOptions->StopAndKillAfter)
        {
            evtNoGuiTargetStoppedByStopAndKillAfter = CreateEvent(0,FALSE,FALSE,0);// unsignaled autoreset
            WaitArray[1] = evtNoGuiTargetStoppedByStopAndKillAfter;
            WaitArraySize++;
        }

        // launch hooking
        if (StartStop())
        {

            // wait for the end of launched application (unexpected unload event) or kill event (if set)
            DWORD dwRet = WaitForMultipleObjects(WaitArraySize,WaitArray,FALSE,INFINITE);
            switch (dwRet)
            {
            case WAIT_OBJECT_0: // hevtUnexpectedUnload
                // clean
                Stop(NULL);
                break;
            //case (WAIT_OBJECT_0+1): // evtNoGuiTargetStoppedByStopAndKillAfter
            //    // stop has already been called by StopHookingAfterTimer --> do nothing
            //    break;
            }
        }

        // close event if created
        if (evtNoGuiTargetStoppedByStopAndKillAfter)
        {
            CloseHandle(evtNoGuiTargetStoppedByStopAndKillAfter);
            evtNoGuiTargetStoppedByStopAndKillAfter = NULL;
        }

        // if user ask to save logs
        if (CommandLineNoGuiSavingFileName)
            SaveLogsThread(CommandLineNoGuiSavingFileName);
    }
    else // gui mode
    {
        // create CSelectWindow object 
        pSelectWindow=new CSelectWindow();
        pSelectWindow->bSelectOnlyDialog=TRUE;
        pSelectWindow->bOwnerWindowSelectable=TRUE;

#ifndef _DEBUG // allow to select WinAPIOverride window easily to debug ApiOverride dll
        pSelectWindow->bMinimizeOwnerWindowWhenSelect=TRUE;
#endif

        // create linked list for analysis messages
        pAnalysisInfoMessages=new CLinkList(sizeof(LOG_LIST_ENTRY));
        pAnalysisInfoMessages->SetHeap(mhLogHeap);


        // load wait cursor
        hWaitCursor=LoadCursor(NULL,IDC_WAIT);

        ///////////////////////////////////////////////////////////////////////////
        // create a thread to avoid CPU usage in case of multiple element selection
        // by refreshing detail view for each item state changed
        //////////////////////////////////////////////////////////////////////////
        // create events associated to thread

        // create an auto reset event with initial state to FALSE
        evtUpdateListView=CreateEvent(NULL,FALSE,FALSE,NULL);
        // create an auto reset event with initial state to TRUE
        evtDetailListViewUpdateUnlocked=CreateEvent(NULL,FALSE,TRUE,NULL);
        // create thread
        hDetailListviewUpdaterThread = CreateThread(NULL,0,DetailListviewUpdater,NULL,0,NULL);

        // set error report for CSupportedParameters
        CSupportedParameters::SetErrorReport(SupportedParametersReportError,NULL);

        CoInitialize(NULL);

        ///////////////////////////////////////////////////////////////////////////
        // show main window
        //////////////////////////////////////////////////////////////////////////
        DialogBox(hInstance, (LPCTSTR)IDD_DIALOGMAIN, NULL, (DLGPROC)WndProc);
    }

    // stop .Net profiling if it is running
    if (pOptions->bNetProfilingEnabled)
    {
        CApiOverride::EnableNETProfilingStatic(FALSE);
    }

    // wait end of DetailListviewUpdater thread if thread has been created
    if (hDetailListviewUpdaterThread)
    {
        if (WaitForSingleObject(hDetailListviewUpdaterThread,10000)!=WAIT_OBJECT_0)
            TerminateThread(hDetailListviewUpdaterThread,(DWORD)-1);
        CloseHandle(hDetailListviewUpdaterThread);
    }

    ///////////////////////////////////////////////////////////////////////////
    // Free memory
    //////////////////////////////////////////////////////////////////////////

    // free hook com dll if loaded
    if (hHookComDll)
        FreeLibrary(hHookComDll);

    // delete pPluginManager
    delete pPluginManager;
    pPluginManager = NULL;

    // delete ProcMon.sys driver interface
    delete pProcMonInterface;

    // delete pApioverrideList
    delete pApiOverrideList;

    //// free all memory allocated by logs
    RemoveAllLogsEntries();

    // high speed way (MSDN : you don't need to destroy allocated memory by calling HeapFree before calling HeapDestroy)
    pLogList->Lock();
    HeapDestroy(mhLogHeap);
    pLogList->ReportHeapDestruction();

    // delete analysis message list
    if (pAnalysisInfoMessages)
    {
        pAnalysisInfoMessages->ReportHeapDestruction();
        delete pAnalysisInfoMessages;
    }
    pLogList->Unlock();

    // delete log list
    delete pLogList;

    // delete pFilters
    delete pFilters;

    // delete CSelectWindow object
    if (pSelectWindow)
        delete pSelectWindow;

    if (bCommandLine)
        // free memory allocated by parsing
        FreeMemoryAllocatedByCommandLineParsing();
    else
    {
        // save options
        pOptions->Save();
    }

    // delete option object
    delete pOptions;

    if (evtDetailListViewUpdateUnlocked)
        CloseHandle(evtDetailListViewUpdateUnlocked);

    CloseHandle(hevtUnexpectedUnload);
    CloseHandle(evtStartStopUnlocked);
    CloseHandle(evtStopping);
    CloseHandle(evtApplicationExit);

    if (!bNoGUI)
        CoUninitialize();

    return 0;
}

//-----------------------------------------------------------------------------
// Name: WndProc
// Object: Main dialog callback
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (bExit)
        return FALSE;

    switch (uMsg)
    {
    case WM_SETCURSOR:
        if (WaitCursorCount>0)
        {
            SetCursor(hWaitCursor);
            //must use SetWindowLongPtr to return value from dialog proc
            SetWindowLongPtr(hWnd, DWLP_MSGRESULT, TRUE);
            return TRUE;
        }
        else
        {
            SetWindowLongPtr(hWnd, DWLP_MSGRESULT, FALSE);
            return FALSE;
        }
        break;
    case WM_INITDIALOG:
        mhWndDialog=hWnd;
        Init();
        break;
    case WM_CLOSE:
        CloseHandle(CreateThread(NULL,NULL,Exit,(LPVOID)LOWORD(wParam),0,NULL));
        break;
    case WM_SIZING:
        CheckSize((RECT*)lParam);
        break;
    case WM_SIZE:
        if (pRebar)
            pRebar->OnSize(wParam,lParam);
        // autosize toolbar (not needed if using rebar)
        //if (pToolbar)    
        //    pToolbar->Autosize();

        switch (wParam)
        {
        case SIZE_MAXIMIZED:
        case SIZE_RESTORED:
            Resize(bUserInterfaceInStartedMode);
            break;
        }
        break;

    case WM_NOTIFY:
        if (pRebar)
        {
            if (pRebar->OnNotify(wParam,lParam))
                break;
        }
        if (pToolbar)
        {
            if (pToolbar->OnNotify(wParam,lParam))
                break;
        }
        //we are only interested in NM_CUSTOMDRAW notifications for ListView
        if (pListview)
        {
            LPNMLISTVIEW pnm;
            pnm = (LPNMLISTVIEW)lParam;

            // check for the Delete key
            if (pnm->hdr.hwndFrom==pListview->GetControlHandle())
            {
                switch (pnm->hdr.code) 
                {
                case LVN_KEYDOWN:
                    {
                        LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN) lParam;
                        ListviewKeyDown(pnkd->wVKey);
                    }
                    break;
                }
            }

            if (pnm->hdr.hwndFrom==ListView_GetHeader(pListview->GetControlHandle()))
            {
                if (pnm->hdr.code==HDN_ITEMCLICK) 
                {
                    // on header click
                    if ((pnm->iItem==CApiOverride::ColumnsIndexId)
                        ||(pnm->iItem==CApiOverride::ColumnsIndexCallDuration))
                        pListview->SetSortingType(CListview::SortingTypeNumber);
                    else
                        pListview->SetSortingType(CListview::SortingTypeString);
                }
            }

            if (pListview->OnNotify(wParam,lParam))
                // if message has been proceed by pListview->OnNotify
                break;

            if (pnm->hdr.hwndFrom == pListview->GetControlHandle() &&
                pnm->hdr.code == NM_CUSTOMDRAW)
            {
                //must use SetWindowLongPtr to return value from dialog proc
                SetWindowLongPtr(hWnd, DWLP_MSGRESULT, (LONG_PTR)ProcessCustomDraw(lParam));
                return TRUE;
            }


            // translate msg to details listview to allow columns sorting and parameter saving
            if (pListviewDetails)
            {
                // in case of right click in details listview, find selected item to adjust menu
                NMHDR* pnmh=((NMHDR*)lParam);
                if (pnmh->hwndFrom==pListviewDetails->GetControlHandle())
                {
                    if (pnmh->code==NM_RCLICK)
                    {   
                        int Index=pListviewDetails->GetSelectedIndex();
                        if ( (Index>0)
                             &&(DetailListViewMinArg<=Index)
                             &&(Index<=DetailListViewMaxArg)
                             )
                        {
                            pListviewDetails->pPopUpMenu->SetEnabledState(ListviewDetailsMenuIDHexDisplay,TRUE);
                        }
                        else if( (Index>0)
                                &&(DetailListViewMinStack<=Index)
                                &&(Index<=DetailListViewMaxStack)
                                )
                        {
                            if (Index>=DetailListViewMinStack+1)
                                // for stack parameters
                                pListviewDetails->pPopUpMenu->SetEnabledState(ListviewDetailsMenuIDHexDisplay,TRUE);
                            else
                                pListviewDetails->pPopUpMenu->SetEnabledState(ListviewDetailsMenuIDHexDisplay,FALSE);
                        }
                        else
                            pListviewDetails->pPopUpMenu->SetEnabledState(ListviewDetailsMenuIDHexDisplay,FALSE);
                    }
                }

                // translate notify event to listview object
                if (pListviewDetails->OnNotify(wParam,lParam))
                    // if message has been proceed by pListview->OnNotify
                    break;

                // translate notify event to listview object
                if (pListviewDetailsTypesDisplay->OnNotify(wParam,lParam))
                    // if message has been proceed by pListview->OnNotify
                    break;

                // custom draw for detail listview
                if (pnm->hdr.hwndFrom == pListviewDetails->GetControlHandle() &&
                                            pnm->hdr.code == NM_CUSTOMDRAW)
                {
                    //must use SetWindowLongPtr to return value from dialog proc
                    SetWindowLongPtr(hWnd, DWL_MSGRESULT, (LONG_PTR)ProcessCustomDrawListViewDetails(lParam));
                    return TRUE;
                }

                // custom draw for detail listview
                if (pnm->hdr.hwndFrom == pListviewDetailsTypesDisplay->GetControlHandle() &&
                    pnm->hdr.code == NM_CUSTOMDRAW)
                {
                    //must use SetWindowLongPtr to return value from dialog proc
                    SetWindowLongPtr(hWnd, DWL_MSGRESULT, (LONG_PTR)ProcessCustomDrawListViewDetails(lParam));
                    return TRUE;
                }
            }
            if (pListViewOverridingFiles)
            {
                // translate notify event to listview object
                if (pListViewOverridingFiles->OnNotify(wParam,lParam))
                    // if message has been proceed by pListview->OnNotify
                    break;
            }
            if (pListViewMonitoringFiles)
            {
                // translate notify event to listview object
                if (pListViewMonitoringFiles->OnNotify(wParam,lParam))
                    // if message has been proceed by pListview->OnNotify
                    break;
            }
            return FALSE;
        }
        break;

    case WM_LBUTTONDOWN:
        OnMouseDown(lParam);
        break;

    case WM_LBUTTONUP:
    case WM_KILLFOCUS:
        OnMouseUp(lParam);
        break;

    case WM_MOUSEMOVE:
        OnMouseMove(lParam);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_START_STOP_HOOKS:
            ThreadedStartStop();
            break;
        case IDC_BUTTON_BROWSE_APP_PATH:
            BrowseApplicationPath();
            break;
        case IDC_BUTTON_PROCESS_FILTERS:
            SetApplicationsFilters();
            break;

        case IDC_BUTTON_BROWSE_MONITORING_FILE:
            BrowseMonitoringFile();
            break;
        case IDC_BUTTON_LOAD_MONITORING_FILE:
            LoadMonitoringFile();
            break;
        case IDC_BUTTON_UNLOAD_MONITORING_FILE:
            UnloadMonitoringFile();
            break;
        case IDC_BUTTON_START_STOP_MONITORING:
            StartStopMonitoring();
            break;

        case IDC_BUTTON_BROWSE_FAKING_FILE:
            BrowseFakingFile();
            break;
        case IDC_BUTTON_LOAD_FAKING_FILE:
            LoadOverridingFile();
            break;
        case IDC_BUTTON_UNLOAD_FAKING_FILE:
            UnloadOverridingFile();
            break;
        case IDC_BUTTON_START_STOP_FAKING:
            StartStopFaking();
            break;

        case IDC_RADIO_ALL_PROCESS:
        case IDC_RADIO_APP_NAME:
        case IDC_RADIO_PID:
            // update pOtions StartWay field
            GetStartWay();

            // allow process filters only if all process option is checked
            if (pOptions->StartWay==COptions::START_WAY_ALL_PROCESSES)
            {
                pToolbar->EnableButton(IDC_BUTTON_PROCESS_FILTERS,TRUE);
                EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_PROCESS_FILTERS),TRUE);
            }
            else
            {
                pToolbar->EnableButton(IDC_BUTTON_PROCESS_FILTERS,FALSE);
                EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_PROCESS_FILTERS),FALSE);
            }

            break;

        case IDC_BUTTON_CLEAR_MONITORING_LISTVIEW:
            ClearLogs(FALSE);
            break;
        case IDC_BUTTON_LOAD_LOGS:
            LoadLogs();
            break;
        case IDC_BUTTON_SAVE_LOGS:
            SaveLogs();
            break;
        case IDC_BUTTON_ABOUT:
            About();
            break;
        case IDC_BUTTON_EXIT:
            CloseHandle(CreateThread(NULL,NULL,Exit,(LPVOID)LOWORD(wParam),0,NULL));
            break;
        case IDC_CHECK_SUSPENDED_MODE:
            if (IsDlgButtonChecked(mhWndDialog,IDC_CHECK_SUSPENDED_MODE)==BST_CHECKED)
                CheckDlgButton(mhWndDialog,IDC_CHECK_START_APP_WAIT_TIME,BST_UNCHECKED);
            break;
        case IDC_CHECK_START_APP_WAIT_TIME:
            if (IsDlgButtonChecked(mhWndDialog,IDC_CHECK_START_APP_WAIT_TIME)==BST_CHECKED)
                CheckDlgButton(mhWndDialog,IDC_CHECK_SUSPENDED_MODE,BST_UNCHECKED);
            break;
        case IDC_CHECK_LOG_ONLY_BASEMODULE:
            LogOnlyBaseModule();
            break;
        case IDC_CHECK_USE_MODULE_LIST:
            LogUseModuleList();
            break;
        case IDC_RADIO_EXCLUSION_LIST:
            if (IsDlgButtonChecked(mhWndDialog,IDC_RADIO_EXCLUSION_LIST)==BST_CHECKED)
                SetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->ExclusionFiltersModulesFileList);
            else
                SetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->InclusionFiltersModulesFileList);
            LogUseModuleList();
            break;
        case IDC_RADIO_INCLUSION_LIST:
            if (IsDlgButtonChecked(mhWndDialog,IDC_RADIO_EXCLUSION_LIST)==BST_CHECKED)
                SetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->ExclusionFiltersModulesFileList);
            else
                SetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->InclusionFiltersModulesFileList);
            LogUseModuleList();
            break;
        case IDC_BUTTON_MODULES_FILTERS:
            SetModulesFilters();
            break;
        case IDC_BUTTON_BROWSE_NOT_LOGGED_MODULE_LIST:
            BrowseNotLoggedModuleList();
            break;
        case IDC_CHECK_FILTERS_APPLY_TO_MONITORING:
            SetMonitoringFiltersState();
            break;
        case IDC_CHECK_FILTERS_APPLY_TO_FAKING:
            SetFakingFiltersState();
            break;
        case IDC_CHECK_START_APP_EXE_IAT_HOOKING:
            {
                BOOL bEnable = !(::IsDlgButtonChecked(mhWndDialog,IDC_CHECK_START_APP_EXE_IAT_HOOKING)==BST_CHECKED);
                ::EnableWindow(::GetDlgItem(mhWndDialog,IDC_CHECK_START_APP_WAIT_TIME),bEnable);
                ::EnableWindow(::GetDlgItem(mhWndDialog,IDC_EDIT_RESUMETIME),bEnable);
            }            
            break;
        case IDC_EDIT_PID:
            // if user manually change pid
            if (HIWORD(wParam)==EN_CHANGE)
            {
                if (bIDC_EDIT_PIDChangedInternally)
                    break;
                // pselect window is non more associated with process id
                pSelectWindow->WindowHandle=0;
            }
            break;
        case IDC_BUTTON_MSDN:
            ShowMSDNOnlineHelp(pListview);
            break;
        case IDC_BUTTON_LAST_ERROR_MESSAGE:
            ShowLastErrorMessage(pListview);
            break;
        case IDC_BUTTON_CONVERT:
            CConvertStringBytes::Show(mhInstance,mhWndDialog);
            break;
        case IDC_BUTTON_HELP:
            Help();
            break;
        case IDC_BUTTON_DONATION:
            MakeDonation();
            break;
        case IDC_BUTTON_MONITORING_FILES_GENERATOR:
            MonitoringFileGenerator();
            break;
        case IDC_BUTTON_SELECT_COLUMNS:
            CSelectColumns::Show();
            break;
        case IDC_BUTTON_OPTIONS:
            ShowOptions();
            break;
        // case IDC_BUTTON_EXPORT_LOGS: // we use menu callback
            // break;
        case IDC_BUTTON_SEARCH_IN_LOGS:
            CSearch::ShowDialog(mhInstance,mhWndDialog);
            break;
        case IDC_BUTTON_STATS:
            CLogsStatsUI::Show(mhInstance,mhWndDialog);
            break;
        case IDC_BUTTON_ANALYSIS:
            CallAnalysis();
            break;
        case IDC_BUTTON_COMPARE_LOGS:
            CompareLogs();
            break;
        case IDC_BUTTON_DUMP:
            Dump();
            break;
        case IDC_BUTTON_MEMORY:
            CMemoryUserInterface::Show(mhInstance,mhWndDialog);
            break;
        case IDC_BUTTON_INTERNAL_CALL:
            CRemoteCall::ShowDialog(mhInstance,mhWndDialog);
            break;
        case IDC_BUTTON_MONITORING_WIZARD:
            MonitoringWizard();
            break;
        case IDC_BUTTON_MONITORING_RELOAD:
            ReloadMonitoring();
            break;
        case IDC_BUTTON_FAKING_RELOAD:
            ReloadOverriding();
            break;
        case IDC_BUTTON_EDIT_MODULE_FILTERS_LIST:
            EditModulesFiltersList();
            break;
        case IDC_BUTTON_UPDATE_MODULE_FILTERS_LIST:
            UpdateModulesFiltersList();
            break;
        case IDC_BUTTON_COM:
            StartStopCOMHooking();
            break;
        case IDC_BUTTON_NET:
            StartStopNETHooking();
            break;
        case IDC_BUTTON_NET_INTERACTION:
            {
                CApiOverride* pApiOverride=GetApiOverrideObject();
                if (pApiOverride)
                    pApiOverride->ShowNetInteractionDialog();
                else
                    MessageBox(mhWndDialog,_T("No process currently hooked"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
            }
            break;
        case IDC_BUTTON_CHECK_FOR_UPDATE:
            CheckForUpdate();
            break;
        case IDC_BUTTON_BUG_REPORT:
            ReportBug();
            break;
        }
        break;

    case WM_DROPFILES:
        OnDropFile(hWnd, (HDROP)wParam);
        break;

    case WM_SHELL_CHANGE_NOTIFY:
        // This is the message that will be sent when changes are detected in the shell namespace
        ShellWatcher.OnChangeMessage(wParam, lParam);
        break; 
    case WM_USER_CROSS_THREAD_SET_FOCUS:
        ::SetFocus((HWND)lParam);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: CallBackToolBarDropDownMenu
// Object: callback called on toolbar popupmenu item clicked
// Parameters :
//     in  : CPopUpMenu* PopUpMenu : clicked popupmenu object associated with the toolbar
//           UINT MenuId : clicked menu ID
//           PVOID UserParam : not used
//     out :
//     return : 
//-----------------------------------------------------------------------------
void CallBackToolBarDropDownMenu(CPopUpMenu* PopUpMenu,UINT MenuId,PVOID UserParam)
{
    UNREFERENCED_PARAMETER(UserParam);
    if (PopUpMenu==pExportPopUpMenu)
    {
        if (MenuId==idMenuExportHTML)
        {
            pListview->Save(pExportPopUpMenu->IsChecked(idMenuExportOnlySelected),
                            pExportPopUpMenu->IsChecked(idMenuExportOnlyVisible),
                            _T("html"),ListViewSpecializedSavingFunction,NULL);
        }
        else if(MenuId==idMenuExportXML)
        {
            pListview->Save(pExportPopUpMenu->IsChecked(idMenuExportOnlySelected),
                            pExportPopUpMenu->IsChecked(idMenuExportOnlyVisible),
                            _T("xml"),ListViewSpecializedSavingFunction,NULL);
        }
        else if(MenuId==idMenuExportTXT)
        {
            pListview->Save(pExportPopUpMenu->IsChecked(idMenuExportOnlySelected),
                            pExportPopUpMenu->IsChecked(idMenuExportOnlyVisible),
                            _T("txt"),ListViewSpecializedSavingFunction,NULL);
        }
        else if(MenuId==idMenuExportCSV)
        {
            pListview->Save(pExportPopUpMenu->IsChecked(idMenuExportOnlySelected),
                            pExportPopUpMenu->IsChecked(idMenuExportOnlyVisible),
                            _T("csv"),ListViewSpecializedSavingFunction,NULL);
        }
        else if(MenuId==idMenuExportOnlySelected)
        {
            // inverse checked state
            pExportPopUpMenu->SetCheckedState(idMenuExportOnlySelected,!pExportPopUpMenu->IsChecked(idMenuExportOnlySelected));
        }
        else if(MenuId==idMenuExportOnlyVisible)
        {
            // inverse checked state
            pExportPopUpMenu->SetCheckedState(idMenuExportOnlyVisible,!pExportPopUpMenu->IsChecked(idMenuExportOnlyVisible));
        }
        else if(MenuId==idMenuExportFullParametersContent)
        {
            // inverse checked state
            pExportPopUpMenu->SetCheckedState(idMenuExportFullParametersContent,!pExportPopUpMenu->IsChecked(idMenuExportFullParametersContent));
        }
    }
    else if (PopUpMenu==pErrorMessagePopUpMenu)
    {
        if (MenuId==idMenuErrorMessageGetLastError)
            ShowLastErrorMessage(pListview);
        else if(MenuId==idMenuErrorMessageReturnedValue)
            ShowReturnErrorMessage(pListview);
    }
    else if (PopUpMenu==pCOMToolsPopUpMenu)
    {
        
        // display CLSID Name convert
        if (MenuId==idMenuComToolsNameCLSIDConvert)
        {
            CComClsidNameConvert::Show(mhInstance,mhWndDialog);
        }
        // display IID Name convert
        else if (MenuId==idMenuComToolsNameIIDConvert)
        {
            CComIidNameConvert::Show(mhInstance,mhWndDialog);
        }
        // display computers known CLSID
        else if (MenuId==idMenuComToolsShowAllCLSID)
        {
            CComComponents::Show(mhInstance,mhWndDialog);
        }
        
        // display relative address and module name for each method
        else if (MenuId==idMenuComToolsShowMethodsAddressInModule)
        {
            if (!hHookComDll)
            {
                TCHAR psz[MAX_PATH];
                CStdFileOperations::GetAppPath(psz,MAX_PATH);
                _tcscat(psz,HOOKCOM_DLL_NAME);
                hHookComDll=LoadLibrary(psz);
                if (!hHookComDll)
                {
                    _stprintf(psz,_T("%s not found"),HOOKCOM_DLL_NAME);
                    MessageBox(mhWndDialog,psz,_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
                    return;
                }
            }
            if (!pHookComShowMethodsAddress)
            {
                pHookComShowMethodsAddress=GetProcAddress(hHookComDll,HOOK_COM_ShowMethodsAddress);
                if (!pHookComShowMethodsAddress)
                {
                    TCHAR psz[MAX_PATH];
                    _stprintf(psz,_T("%s not found in %s"),HOOK_COM_ShowMethodsAddressT,HOOKCOM_DLL_NAME);
                    MessageBox(mhWndDialog,psz,_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
                    return;
                }
            }

            pHookComShowMethodsAddress();
        }
        // display com interaction
        else if (MenuId==idMenuComToolsHookedComObjectInteraction)
        {
            CApiOverride* pApiOverride;
            pApiOverride=GetApiOverrideObject();
            if (!pApiOverride)
            {
                MessageBox(mhWndDialog,_T("No process currently hooked"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
                return;
            }

            pApiOverride->ShowCOMInteractionDialog();
        }
        else if  (MenuId==idMenuComToolsMonitoringInterfaceWizard)
        {
            CMonitoringWizard pobj(CMonitoringWizard::MonitoringWizardType_COM);
            if (pobj.ShowDialog(mhInstance,mhWndDialog)!=MONITORING_WIZARD_DLG_RES_OK)
                return;
        }
    }
    else if (PopUpMenu == pPluginsPopUpMenu)
    {
        pPluginManager->OnMenuClick(MenuId);
    }
}

//-----------------------------------------------------------------------------
// Name: CallBackLogListviewItemSelection
// Object: callback called on log selection in listview
// Parameters :
//     in  : int ItemIndex : index of selected item in log listview
//           int SubItemIndex : subitem index of selected item in log listview
//           PVOID UserParam : not used
//     out :
//     return : 
//-----------------------------------------------------------------------------
void CallBackLogListviewItemSelection(int ItemIndex,int SubItemIndex,LPVOID UserParam)
{
    UNREFERENCED_PARAMETER(UserParam);
    UNREFERENCED_PARAMETER(SubItemIndex);

    // Removing logs generate selection change, that generate an item change event --> avoid this while items are removed
    if  ( (!bRemovingSelectedLogs) && (!RemovingAllLogsEntries) && (!hThreadCallAnalysis) )
    {
        LastSelectedItemIndexInMainView=ItemIndex;
        SetEvent(evtUpdateListView);
    }
}

//-----------------------------------------------------------------------------
// Name: DetailListviewUpdater
// Object: 
// Parameters :
//     in  : lpParameter : unused
//     out :
//     return : 
//-----------------------------------------------------------------------------
DWORD WINAPI DetailListviewUpdater(LPVOID lpParameter)
{
    UNREFERENCED_PARAMETER(lpParameter);

    HANDLE ph[2];
    DWORD dwRes;

    ph[0]=evtUpdateListView;
    ph[1]=evtApplicationExit;

    CoInitialize(NULL);

    // assume Dia dll is registered (avoid multiple registrations / unregistrations)
    TCHAR DiaDllPath[MAX_PATH];
    BOOL bDiaDllbAlreadyRegister=FALSE;
    CStdFileOperations::GetAppPath(DiaDllPath,MAX_PATH);
    _tcscat(DiaDllPath,DIA_DLL_NAME);
    CRegisterComComponent::RegisterComponentIfNeeded(DiaDllPath,__uuidof(DiaSource),&bDiaDllbAlreadyRegister);

    for(;;)
    {
        // wait for an update event or application exit
        dwRes=WaitForMultipleObjects(2,ph,FALSE,INFINITE);
        if (dwRes!=WAIT_OBJECT_0)// stop event or error
            break;

        // until update event are raised in less than 100 ms
        // avoid to update detail listview for each item selection change
        // when user do a multiple selection --> do it only for the last
        while (WaitForSingleObject(evtUpdateListView,100)!=WAIT_TIMEOUT)
        {
            // do nothing (avoid detail listview refresh)
        }

        if (!bExit)
        {
            // sleep in case of right click to enable TrackPopUpMenu to be executed before UpdateDetailListView
            // and as during TrackPopUpMenu call, SetCursor is not taken into account, 
            // wait cursor can be display during all popup menu showing
            Sleep(100);
            // refresh detail listview with last selected item
            UpdateDetailListView(LastSelectedItemIndexInMainView);
        }
    }

    CloseHandle(evtUpdateListView);
    evtUpdateListView=NULL;

    if (!bDiaDllbAlreadyRegister)
        CRegisterComComponent::UnregisterComponent(DiaDllPath);

    CoUninitialize();

    return 0;
}
//-----------------------------------------------------------------------------
// Name: UpdateDetailListView
// Object: refresh the content of the detail listview according to ItemIndex
// Parameters :
//     in  : int ItemIndex : index of selected item in log listview
//     out :
//     return : 
//-----------------------------------------------------------------------------
void UpdateDetailListView(int ItemIndex)
{
    // retrieve item id
    // find associated item in pLogList
    LOG_LIST_ENTRY* pLogEntry=NULL;

    // assume no item will be removed during update
    ::WaitForSingleObject(evtDetailListViewUpdateUnlocked,INFINITE);

    // if no item removal occurred during previous wait operation
    if (LastSelectedItemIndexInMainView<0)
        goto CleanUp;

    if(!pListview->GetItemUserData(ItemIndex,(LPVOID*)(&pLogEntry)))
        goto CleanUp;
    if (pLogEntry==0)
        goto CleanUp;

    ::UpdateDetailListView(pLogEntry);

    // release event
CleanUp:
    ::SetEvent(evtDetailListViewUpdateUnlocked);
}

//-----------------------------------------------------------------------------
// Name: UpdateDetailListView
// Object: refresh the content of the detail listview according to pLogEntry
// Parameters :
//     in  : LOG_LIST_ENTRY* pLogEntry : Log entry
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL UpdateDetailListView(LOG_LIST_ENTRY* pLogEntry)
{
    TCHAR psz[2*MAX_PATH];
    TCHAR psz2[50];
    DWORD NbMaxCallStackParam;
    CLinkListItem* pItem;
    TCHAR* ParamString;
    BYTE Cnt;
    BYTE Cnt2;
    CLinkListSimple* pParamFields;

    if (IsBadReadPtr(pLogEntry,sizeof(LOG_LIST_ENTRY)))
    {
        SetEvent(evtDetailListViewUpdateUnlocked);
        return FALSE;
    }

    if (pLogEntry->Type!=ENTRY_LOG)
    {
        SetEvent(evtDetailListViewUpdateUnlocked);
        return FALSE;
    }

    if (IsBadReadPtr(pLogEntry->pLog,sizeof(LOG_ENTRY)))
    {
        SetEvent(evtDetailListViewUpdateUnlocked);
        return FALSE;
    }

    SetWaitCursor(TRUE);

    pLogListviewDetails=pLogEntry->pLog;

    DetailListViewMinArg=-1;
    DetailListViewMaxArg=-1;
    DetailListViewMinStack=-1;
    DetailListViewMaxStack=-1;

    // user select a log so show details of log
    int Index=0;
    pListviewDetails->Clear();
    pListviewDetailsTypesDisplay->Clear();
    pListviewDetails->SetItemText(Index,0,_T("Api Name"));
    pListviewDetails->SetItemText(Index,1,pLogListviewDetails->pszApiName);
    Index++;

    switch (pLogListviewDetails->pHookInfos->HookType)
    {
    case HOOK_TYPE_COM:
        {
            TCHAR pszGUID[128];
            CGUIDStringConvert::TcharFromCLSID(&pLogListviewDetails->HookTypeExtendedFunctionInfos.InfosForCOM.ClassID,
                                                pszGUID);
            // add CLSID only if defined
            // CLSID is known when CLSID != IID (see hookcom dll code)
            if (CGUIDStringConvert::IsKnownCLSID(pszGUID))
            {
                pListviewDetails->SetItemText(Index,0,_T("Object CLSID"));
                if (CGUIDStringConvert::GetClassName(&pLogListviewDetails->HookTypeExtendedFunctionInfos.InfosForCOM.ClassID,psz,_countof(psz)-_countof(pszGUID)-2))
                {
                    _tcscat(psz,_T(" "));
                    _tcscat(psz,pszGUID);
                    pListviewDetails->SetItemText(Index,1,psz);
                }
                else
                {
                    pListviewDetails->SetItemText(Index,1,pszGUID);
                }
            }
            else if (CGUIDStringConvert::IsKnownIID(pszGUID))
            {
                pListviewDetails->SetItemText(Index,0,_T("Default Object Of"));
                if (CGUIDStringConvert::GetInterfaceName(pszGUID,psz,_countof(psz)-_countof(pszGUID)-2))
                {
                    _tcscat(psz,_T(" "));
                    _tcscat(psz,pszGUID);
                    pListviewDetails->SetItemText(Index,1,psz);
                }
                else
                {
                    pListviewDetails->SetItemText(Index,1,pszGUID);
                }
            }
            else
            {
                pListviewDetails->SetItemText(Index,0,_T("Object CLSID"));   
                pListviewDetails->SetItemText(Index,1,pszGUID);
            }
            Index++;

            pListviewDetails->SetItemText(Index,0,_T("Interface ID"));
            CGUIDStringConvert::TcharFromIID(
                                            &pLogListviewDetails->HookTypeExtendedFunctionInfos.InfosForCOM.InterfaceID,
                                            pszGUID
                                            );
            if (CGUIDStringConvert::GetInterfaceName(pszGUID,psz,_countof(psz)-_countof(pszGUID)-2))
            {
                _tcscat(psz,_T(" "));
                _tcscat(psz,pszGUID);
                pListviewDetails->SetItemText(Index,1,psz);
            }
            else
            {
                pListviewDetails->SetItemText(Index,1,pszGUID);
            }
            Index++;

            pListviewDetails->SetItemText(Index,0,_T("VTBL Index"));
            _stprintf(psz,_T("0x%08X"),pLogListviewDetails->HookTypeExtendedFunctionInfos.InfosForCOM.VTBLIndex);
            pListviewDetails->SetItemText(Index,1,psz);
            Index++;
        }
        break;
    case HOOK_TYPE_NET:
        pListviewDetails->SetItemText(Index,0,_T("Function Token"));
        _stprintf(psz,_T("0x%08X"),pLogListviewDetails->HookTypeExtendedFunctionInfos.InfosForNET.FunctionToken);
        pListviewDetails->SetItemText(Index,1,psz);
        Index++;
        break;
    //case HOOK_TYPE_API:
    //    break;
    }


    pListviewDetails->SetItemText(Index,0,_T("Module Name"));
    pListviewDetails->SetItemText(Index,1,pLogListviewDetails->pszModuleName);
    Index++;

    pListviewDetails->SetItemText(Index,0,_T("Calling Module"));
    pListviewDetails->SetItemText(Index,1,pLogListviewDetails->pszCallingModuleName);
    Index++;

    // empty line
    pListviewDetails->SetItemText(Index,0,_T(""));
    Index++;

    // Call time
    // Copy the time into a quadword.
    ULONGLONG ul;
    ul = (((ULONGLONG) pLogListviewDetails->pHookInfos->CallTime.dwHighDateTime) << 32) + pLogListviewDetails->pHookInfos->CallTime.dwLowDateTime;
    int Nano100s=(int)(ul%10);
    int MicroSeconds=(int)((ul/10)%1000);
    int MilliSeconds=(int)((ul/10000)%1000);
    int Seconds=(int)((ul/_SECOND)%60);
    int Minutes=(int)((ul/_MINUTE)%60);
    int Hours=(int)((ul/_HOUR)%24);
    _sntprintf(psz,MAX_PATH,_T("%.2u:%.2u:%.2u:%.3u:%.3u,%.1u"),
                        Hours,
                        Minutes,
                        Seconds,
                        MilliSeconds,
                        MicroSeconds,
                        Nano100s
                        );

    pListviewDetails->SetItemText(Index,0,_T("Start Time"));
    pListviewDetails->SetItemText(Index,1,psz);
    Index++;

    // Call duration
    if (pLogListviewDetails->pHookInfos->bParamDirectionType!=PARAM_DIR_TYPE_IN_NO_RETURN)
    {
        _sntprintf(psz,MAX_PATH,_T("%u"),pLogListviewDetails->pHookInfos->dwCallDuration);
        pListviewDetails->SetItemText(Index,0,_T("Duration (us)"));
        pListviewDetails->SetItemText(Index,1,psz);
        Index++;
    }

    // process
    _sntprintf(psz,MAX_PATH,_T("0x%X (%u)"),pLogListviewDetails->pHookInfos->dwProcessId,pLogListviewDetails->pHookInfos->dwProcessId);
    pListviewDetails->SetItemText(Index,0,_T("Process Id"));
    pListviewDetails->SetItemText(Index,1,psz);
    Index++;

    // thread
    _sntprintf(psz,MAX_PATH,_T("0x%X (%u)"),pLogListviewDetails->pHookInfos->dwThreadId,pLogListviewDetails->pHookInfos->dwThreadId);
    pListviewDetails->SetItemText(Index,0,_T("Thread Id"));
    pListviewDetails->SetItemText(Index,1,psz);
    Index++;

    // origin address
    _sntprintf(psz,MAX_PATH,_T("0x%p") ,pLogListviewDetails->pHookInfos->pOriginAddress);
    pListviewDetails->SetItemText(Index,0,_T("Caller Address"));
    pListviewDetails->SetItemText(Index,1,psz);
    Index++;

    // caller relative address
    if (pLogListviewDetails->pHookInfos->RelativeAddressFromCallingModuleName)
    {
        _sntprintf(psz,MAX_PATH,_T("0x%p"),pLogListviewDetails->pHookInfos->RelativeAddressFromCallingModuleName);
        pListviewDetails->SetItemText(Index,0,_T("Caller Relative Address"));
        pListviewDetails->SetItemText(Index,1,psz);
        Index++;
    }

    // empty line
    pListviewDetails->SetItemText(Index,0,_T(""));
    Index++;

    // param direction
    switch (pLogListviewDetails->pHookInfos->bParamDirectionType)
    {
    case PARAM_DIR_TYPE_IN:
        _tcscpy(psz,_T("In"));
        break;
    case PARAM_DIR_TYPE_OUT:
        _tcscpy(psz,_T("Out"));
        break;
    case PARAM_DIR_TYPE_IN_NO_RETURN:
        _tcscpy(psz,_T("In No Return"));
        break;
    }
    pListviewDetails->SetItemText(Index,0,_T("Logging Type"));
    pListviewDetails->SetItemText(Index,1,psz);
    Index++;

    DetailListViewMinArg=Index;

    // params
    for (Cnt=0;Cnt<pLogListviewDetails->pHookInfos->bNumberOfParameters;Cnt++)
    {
        _stprintf(psz,_T("Parameter %u"),Cnt+1);
        pListviewDetails->SetItemText(Index,0,psz);

        // translate param to string
        CSupportedParameters::ParameterToString(pLogListviewDetails->pszModuleName,&pLogListviewDetails->ParametersInfoArray[Cnt],&ParamString,TRUE);
        if (CSupportedParameters::SplitParameterFields(ParamString,pLogListviewDetails->ParametersInfoArray[Cnt].dwType,&pParamFields))
        {
            // for each field
            for(pItem=pParamFields->Head;pItem;pItem=pItem->NextItem)
            {
                pListviewDetails->SetItemText(Index,1,(TCHAR*)pItem->ItemData);
                // increment list view line index
                Index++;
            }
            CSupportedParameters::FreeSplittedParameterFields(&pParamFields);
        }
        else
        {
            pListviewDetails->SetItemText(Index,1,ParamString);
            Index++;
        }
        // free string allocated by ParameterToString
        delete ParamString;
    }

    DetailListViewMaxArg=Index;

    // if no parameters
    if (DetailListViewMinArg==DetailListViewMaxArg)
    {
        DetailListViewMinArg=-1;
        DetailListViewMaxArg=-1;
    }

    if (pLogListviewDetails->pHookInfos->bParamDirectionType!=PARAM_DIR_TYPE_IN_NO_RETURN)
    {
        // empty line
        pListviewDetails->SetItemText(Index,0,_T(""));
        Index++;

        // ret value
        _sntprintf(psz,MAX_PATH,_T("0x%p"),pLogListviewDetails->pHookInfos->ReturnValue);
        pListviewDetails->SetItemText(Index,0,_T("Returned Value"));
        pListviewDetails->SetItemText(Index,1,psz);
        DetailListViewReturnValueIndex = Index;
        Index++;

        // floating ret value
        _stprintf(psz,_T("%.19g"), pLogListviewDetails->pHookInfos->DoubleResult);
        pListviewDetails->SetItemText(Index,0,_T("Floating Return"));
        pListviewDetails->SetItemText(Index,1,psz);
        Index++;

        // last error
        _sntprintf(psz,MAX_PATH,_T("0x%.8X"),pLogListviewDetails->pHookInfos->dwLastError);
        pListviewDetails->SetItemText(Index,0,_T("Last Error Value"));
        pListviewDetails->SetItemText(Index,1,psz);
        DetailListViewLastErrorIndex = Index;
        Index++;
    }
    else
    {
        DetailListViewReturnValueIndex = -1;
        DetailListViewLastErrorIndex = -1;
    }



    // empty line
    pListviewDetails->SetItemText(Index,0,_T(""));
    Index++;

    // registers before call
    _sntprintf(psz,
                MAX_PATH,
                _T("EAX=0x%.8X, EBX=0x%.8X, ECX=0x%.8X, EDX=0x%.8X, ESI=0x%.8X, EDI=0x%.8X, EFL=0x%.8X, ESP=0x%.8X, EBP=0x%.8X"),
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.eax,
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.ebx,
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.ecx,
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.edx,
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.esi,
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.edi,
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.efl,
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.esp,
                pLogListviewDetails->pHookInfos->RegistersBeforeCall.ebp
                );
    pListviewDetails->SetItemText(Index,0,_T("Registers Before Call"));
    pListviewDetails->SetItemText(Index,1,psz);
    Index++;

    // registers after call
    if (pLogListviewDetails->pHookInfos->bParamDirectionType!=PARAM_DIR_TYPE_IN_NO_RETURN)
    {
        _sntprintf(psz,
                MAX_PATH,
                _T("EAX=0x%.8X, EBX=0x%.8X, ECX=0x%.8X, EDX=0x%.8X, ESI=0x%.8X, EDI=0x%.8X, EFL=0x%.8X, ESP=0x%.8X, EBP=0x%.8X"),
                pLogListviewDetails->pHookInfos->RegistersAfterCall.eax,
                pLogListviewDetails->pHookInfos->RegistersAfterCall.ebx,
                pLogListviewDetails->pHookInfos->RegistersAfterCall.ecx,
                pLogListviewDetails->pHookInfos->RegistersAfterCall.edx,
                pLogListviewDetails->pHookInfos->RegistersAfterCall.esi,
                pLogListviewDetails->pHookInfos->RegistersAfterCall.edi,
                pLogListviewDetails->pHookInfos->RegistersAfterCall.efl,
                pLogListviewDetails->pHookInfos->RegistersAfterCall.esp,
                pLogListviewDetails->pHookInfos->RegistersAfterCall.ebp
                );
        pListviewDetails->SetItemText(Index,0,_T("Registers After Call"));
        pListviewDetails->SetItemText(Index,1,psz);
        Index++;
    }

    // call stack
    if (pLogListviewDetails->pHookInfos->CallStackSize==0)
    {
        // if call stack parameters columns, 
        if (pListviewDetails->GetColumnCount()==4)
        {
            // remove them
            pListviewDetails->RemoveColumn(3);
            pListviewDetails->RemoveColumn(2);
        }
    }
    else
    {
        // if params are not retrieved
        if (pLogListviewDetails->pHookInfos->CallStackEbpRetrievalSize==0)
        {
            // if call stack parameters columns, 
            if (pListviewDetails->GetColumnCount()==4)
            {
                // remove parameter column them
                pListviewDetails->RemoveColumn(3);
            }
        }
        // if not call stack parameters columns, 
        else if (pListviewDetails->GetColumnCount()<4)
        {
            // add them
            pListviewDetails->AddColumn(_T(""),150,LVCFMT_LEFT);
            pListviewDetails->AddColumn(_T(""),300,LVCFMT_LEFT);
        }
        // empty line
        pListviewDetails->SetItemText(Index,0,_T(""));
        Index++;

        DetailListViewMinStack=Index;
    
        pListviewDetails->SetItemText(Index,0,_T("Call Stack"));
        pListviewDetails->SetItemText(Index,1,_T(""));
        pListviewDetails->SetItemText(Index,2,_T("Possible Func Name"));
        pListviewDetails->SetItemText(Index,3,_T("Stack Parameters"));
        Index++;

        CPE** PeArray=new CPE*[pLogListviewDetails->pHookInfos->CallStackSize];
        CPE*  pPe;
        TCHAR PeFileName[MAX_PATH];
        DWORD PeArrayUsedSize=0;

        CDebugInfos** DebugInfosArray=new CDebugInfos*[pLogListviewDetails->pHookInfos->CallStackSize];
        CDebugInfos* pDebugInfos;
        DWORD DebugInfosArrayUsedSize=0;

        // add other stack items
        for (Cnt=0;Cnt<pLogListviewDetails->pHookInfos->CallStackSize;Cnt++)
        {
            //////////////////////////////////////////////
            // Display module name and relative address
            //////////////////////////////////////////////
            if (pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName)
            {
                if (pLogListviewDetails->CallSackInfoArray[Cnt].RelativeAddress)
                {
                    _sntprintf(psz,
                        MAX_PATH,
                        _T("0x%p   %s + 0x%p"),
                        pLogListviewDetails->CallSackInfoArray[Cnt].Address,
                        pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName,
                        pLogListviewDetails->CallSackInfoArray[Cnt].RelativeAddress
                        );
                }
                else
                {
                    _sntprintf(psz,
                        MAX_PATH,
                        _T("0x%p   %s"),
                        pLogListviewDetails->CallSackInfoArray[Cnt].Address,
                        pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName
                        );
                }
            }
            else
            {
                _sntprintf(psz,
                    MAX_PATH,
                    _T("0x%p"),
                    pLogListviewDetails->CallSackInfoArray[Cnt].Address
                    );
            }
            pListviewDetails->SetItemText(Index,1,psz);

            if (pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName)
            {
                //////////////////////////////////////////////
                // try to get func name
                //////////////////////////////////////////////

                // 1) try to get infos from debug file if any

                // check if debug file has been parsed
                pDebugInfos=NULL;
                for (Cnt2=0;Cnt2<DebugInfosArrayUsedSize;Cnt2++)
                {
                    if (_tcsicmp(DebugInfosArray[Cnt2]->GetFileName(),pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName)==0)
                    {
                        pDebugInfos=DebugInfosArray[Cnt2];
                        break;
                    }
                }
                if (pDebugInfos==NULL)
                {
                    // debug file has not been parsed
                    pDebugInfos=new CDebugInfos(pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName,FALSE);

                    // add pDebugInfos to DebugInfosArray
                    DebugInfosArray[DebugInfosArrayUsedSize]=pDebugInfos;
                    DebugInfosArrayUsedSize++;
                }
                CFunctionInfos* pFunctionInfos = NULL;
                if (pDebugInfos->HasDebugInfos())
                {
                    pDebugInfos->FindFunctionByRVA((ULONGLONG)pLogListviewDetails->CallSackInfoArray[Cnt].RelativeAddress,&pFunctionInfos);
                }
                if (pFunctionInfos)
                {
                    pListviewDetails->SetItemText(Index,2,pFunctionInfos->Name);
                    delete pFunctionInfos;
                }
                else
                {
                    // 2) try to find function name with dll exports

                    // assume module is a dll
                    if (CStdFileOperations::DoesExtensionMatch(pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName,_T("dll")))
                    {
                        // check if PE of dll has already been parsed
                        pPe=NULL;
                        for (Cnt2=0;Cnt2<PeArrayUsedSize;Cnt2++)
                        {
                            PeArray[Cnt2]->GetFileName(PeFileName);
                            if (_tcsicmp(PeFileName,pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName)==0)
                            {
                                pPe=PeArray[Cnt2];
                                break;
                            }
                        }
                        if(pPe==NULL)
                        {
                            // pe has not been parsed
                            // --> parse it's export table
                            pPe=new CPE(pLogListviewDetails->CallSackInfoArray[Cnt].pszModuleName);
                            pPe->Parse(TRUE,FALSE);

                            // add pPe to PeArray
                            PeArray[PeArrayUsedSize]=pPe;
                            PeArrayUsedSize++;
                        }

                        // find the nearest lower exported function
                        CPE::EXPORT_FUNCTION_ITEM* pNearestLower=NULL;
                        CLinkListItem* pItem;
                        
                        CPE::EXPORT_FUNCTION_ITEM* pCurrent;
                        for(pItem=pPe->pExportTable->Head;pItem;pItem=pItem->NextItem)
                        {
                            pCurrent=(CPE::EXPORT_FUNCTION_ITEM*)pItem->ItemData;

                            // if relative address of function is less than called address
                            if (pCurrent->FunctionAddressRVA<=(UINT_PTR)pLogListviewDetails->CallSackInfoArray[Cnt].RelativeAddress)
                            {
                                // if pNearestLower has already been defined
                                if (pNearestLower==NULL)
                                    pNearestLower=pCurrent;
                                // if relative address is upper than pNearestLower address
                                else if (pCurrent->FunctionAddressRVA>(DWORD)pNearestLower->FunctionAddressRVA)
                                    pNearestLower=pCurrent;
                            }
                        }

                        // check if nearest lower exported function has a name
                        if (pNearestLower)
                            if (*pNearestLower->FunctionName==0)
                                // if no name --> put ordinal value
                                _stprintf(pNearestLower->FunctionName,_T("Ordinal 0x") _T("%X") ,pNearestLower->ExportedOrdinal);

                        if (pNearestLower)
                            pListviewDetails->SetItemText(Index,2,pNearestLower->FunctionName);
                        else
                            pListviewDetails->SetItemText(Index,2,_T("Func not exported"));
                    }
                } // end of function name finding with dll export parsing
                //////////////////////////////////////////////
                // end try to get func name
                //////////////////////////////////////////////
            }

            //////////////////////////////////////////////
            // display parameters
            //////////////////////////////////////////////
            if (pLogListviewDetails->CallSackInfoArray[Cnt].Parameters)
            {
                // param need 20 TCHAR to be displayed (as it only be displayed as an hex DWORD)
                NbMaxCallStackParam=2*MAX_PATH/20;
                // empty psz
                *psz=0;
                for ( Cnt2=0;
                      (Cnt2<(pLogListviewDetails->pHookInfos->CallStackEbpRetrievalSize/sizeof(PBYTE)))
                       && (Cnt2<NbMaxCallStackParam);
                      Cnt2++
                     )
                {
                    // print param
                    _stprintf(psz2,_T("Param%u=0x%p"),Cnt2+1,*(PBYTE*)&pLogListviewDetails->CallSackInfoArray[Cnt].Parameters[Cnt2*sizeof(PBYTE)]);
                    // if not first param, add separator
                    if (Cnt2>0)
                        _tcscat(psz,_T(", "));
                    // add param to all params string
                    _tcscat(psz,psz2);
                }
                pListviewDetails->SetItemText(Index,3,psz);
            }
            Index++;
        }
        // free memory

        for (Cnt2=0;Cnt2<DebugInfosArrayUsedSize;Cnt2++)
        {
            delete DebugInfosArray[Cnt2];
        }
        delete[] DebugInfosArray;

        for (Cnt2=0;Cnt2<PeArrayUsedSize;Cnt2++)
        {
            delete PeArray[Cnt2];
        }
        delete[] PeArray;

        // fill DetailListViewMaxStack for context menu
        DetailListViewMaxStack=Index;
    }

    SetWaitCursor(FALSE);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: GetStartWay
// Object: get start way from user interface, and update pOptions->StartWay field
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
COptions::tagStartWay GetStartWay()
{
    if (IsDlgButtonChecked(mhWndDialog,IDC_RADIO_ALL_PROCESS)==BST_CHECKED)
        pOptions->StartWay=COptions::START_WAY_ALL_PROCESSES;
    else if (IsDlgButtonChecked(mhWndDialog,IDC_RADIO_APP_NAME)==BST_CHECKED)
        pOptions->StartWay=COptions::START_WAY_LAUNCH_APPLICATION;
    else
        pOptions->StartWay=COptions::START_WAY_PID;

    return pOptions->StartWay;
}

//-----------------------------------------------------------------------------
// Name: GetOptionsFromGUI
// Object: get options from user interface
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL GetOptionsFromGUI()
{
    BOOL bSuccess;

    GetStartWay();

    // Retrieve PID we want to hook from dialog interface
    TCHAR psz[MAX_PATH];
    *psz=0;
    pOptions->StartupApplicationPID=0;
    SendMessage(GetDlgItem(mhWndDialog,IDC_EDIT_PID),(UINT) WM_GETTEXT,MAX_PATH,(LPARAM)psz);
    if(_tcsnicmp(psz,_T("0x"),2)==0)
        _stscanf(psz+2,_T("%X"),&pOptions->StartupApplicationPID);
    else
        _stscanf(psz,_T("%u"),&pOptions->StartupApplicationPID);


    // get Application full path
    GetDlgItemText(mhWndDialog,IDC_EDIT_APP_PATH,pOptions->StartupApplicationPath,MAX_PATH);

    // get cmd line
    GetDlgItemText(mhWndDialog,IDC_EDIT_CMD_LINE,pOptions->StartupApplicationCmd,MAX_PATH);

    // if exe iat hooking checked
    pOptions->bExeIATHooks=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_START_APP_EXE_IAT_HOOKING)==BST_CHECKED);

    // if wait time before injection checked
    pOptions->StartupApplicationbOnlyAfter=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_START_APP_WAIT_TIME)==BST_CHECKED);

    // retrieve resume time from dialog interface
    pOptions->StartupApplicationOnlyAfterMs=GetDlgItemInt(mhWndDialog,IDC_EDIT_RESUMETIME,&bSuccess,FALSE);

    // stop and kill
    pOptions->StopAndKillAfter=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_STOP_AND_KILL_APP)==BST_CHECKED);
    pOptions->StopAndKillAfterMs=GetDlgItemInt(mhWndDialog,IDC_EDIT_STOP_AND_KILL_APP_TIME,&bSuccess,FALSE);

    // if wait time before injection for all processes checked
    pOptions->StartAllProcessesbOnlyAfter=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_INJECT_IN_ALL_ONLY_AFTER)==BST_CHECKED);

    // wait time before injection for all processes
    pOptions->StartAllProcessesOnlyAfterMs=GetDlgItemInt(mhWndDialog,IDC_EDIT_INSERT_AFTER,&bSuccess,FALSE);

    // update filters applies to monitoring
    pOptions->bFiltersApplyToMonitoring=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_FILTERS_APPLY_TO_MONITORING)==BST_CHECKED);

    // update filters applies to faking
    pOptions->bFiltersApplyToOverriding=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_FILTERS_APPLY_TO_FAKING)==BST_CHECKED);

    // update only base module
    pOptions->bOnlyBaseModule=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_LOG_ONLY_BASEMODULE)==BST_CHECKED);

    // update use not hooked module list
    pOptions->bUseFilterModulesFileList=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_USE_MODULE_LIST)==BST_CHECKED);
    pOptions->bFilterModulesFileListIsExclusionList=(IsDlgButtonChecked(mhWndDialog,IDC_RADIO_EXCLUSION_LIST)==BST_CHECKED);

    // update not hooked module list
    if (pOptions->bFilterModulesFileListIsExclusionList)
        GetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->ExclusionFiltersModulesFileList,MAX_PATH);
    else
        GetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->InclusionFiltersModulesFileList,MAX_PATH);

    // update ExportOnlySelected state
    pOptions->ExportOnlySelected=pExportPopUpMenu->IsChecked(idMenuExportOnlySelected);

    // update ExportOnlyVisible state
    pOptions->ExportOnlyVisible=pExportPopUpMenu->IsChecked(idMenuExportOnlyVisible);

    // update ExportFullParametersContent state
    pOptions->ExportFullParametersContent=pExportPopUpMenu->IsChecked(idMenuExportFullParametersContent);

    // store ComEnable hooking
    pOptions->bComAutoHookingEnabled=(pToolbar->GetButtonState(IDC_BUTTON_COM)&TBSTATE_CHECKED);

    // store enable .Net hooking
    pOptions->bNetProfilingEnabled=(pToolbar->GetButtonState(IDC_BUTTON_NET)&TBSTATE_CHECKED);

    int ColumnsOrder[CApiOverride::NbColumns];
#ifdef _DEBUG
    if(
#endif
    ListView_GetColumnOrderArray(pListview->GetControlHandle(),CApiOverride::NbColumns,ColumnsOrder)
#ifdef _DEBUG
    == FALSE)
    ::DebugBreak(); // bad number of columns, columns order saving will fail
#else
    ;
#endif

    for (int cnt=CApiOverride::FirstColumn;cnt<=CApiOverride::LastColumn;cnt++)
    {
        if (pOptions->ListviewLogsColumnsInfos[cnt].bVisible)
            pOptions->ListviewLogsColumnsInfos[cnt].Size = pListview->GetColumnWidth(cnt);

        pOptions->ListviewLogsColumnsInfos[cnt].iOrder = ColumnsOrder[cnt];
    }

    return TRUE;
}
// plugin use only
COptions* GetOptions()
{
    return pOptions;
}
//-----------------------------------------------------------------------------
// Name: SetGUIFromOptions
// Object: set user interface according to options 
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL SetGUIFromOptions()
{
    WPARAM wParam;
    // check radio according to option start way
    int nId=IDC_RADIO_PID;
    if (pOptions->StartWay==COptions::START_WAY_LAUNCH_APPLICATION)
        nId=IDC_RADIO_APP_NAME;
    else if(pOptions->StartWay==COptions::START_WAY_ALL_PROCESSES)
        nId=IDC_RADIO_ALL_PROCESS;
    
    // uncheck all radio buttons
    CDialogHelper::SetButtonCheckState(mhWndDialog,IDC_RADIO_PID,FALSE);
    CDialogHelper::SetButtonCheckState(mhWndDialog,IDC_RADIO_APP_NAME,FALSE);
    CDialogHelper::SetButtonCheckState(mhWndDialog,IDC_RADIO_ALL_PROCESS,FALSE);

    // check the specified radio button
    CDialogHelper::SetButtonCheckState(mhWndDialog,nId,TRUE);

    // allow or not process filtering
    if (!bNoGUI)
    {
        if (pOptions->StartWay==COptions::START_WAY_ALL_PROCESSES)
        {
            pToolbar->EnableButton(IDC_BUTTON_PROCESS_FILTERS,TRUE);
            EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_PROCESS_FILTERS),TRUE);
        }
        else
        {
            pToolbar->EnableButton(IDC_BUTTON_PROCESS_FILTERS,FALSE);
            EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_PROCESS_FILTERS),FALSE);
        }
    }

    // restore pid value
    if (pOptions->StartupApplicationPID!=0)
        SetDlgItemInt(mhWndDialog,IDC_EDIT_PID,pOptions->StartupApplicationPID,FALSE);

    // set Application full path
    SetDlgItemText(mhWndDialog,IDC_EDIT_APP_PATH,pOptions->StartupApplicationPath);

    // set cmd line
    SetDlgItemText(mhWndDialog,IDC_EDIT_CMD_LINE,pOptions->StartupApplicationCmd);

    // if exe iat hooking checked
    wParam=pOptions->bExeIATHooks?BST_CHECKED:BST_UNCHECKED;
    SendDlgItemMessage(mhWndDialog,IDC_CHECK_START_APP_EXE_IAT_HOOKING,(UINT)BM_SETCHECK,wParam,0);
    ::EnableWindow(::GetDlgItem(mhWndDialog,IDC_CHECK_START_APP_WAIT_TIME),!pOptions->bExeIATHooks);
    ::EnableWindow(::GetDlgItem(mhWndDialog,IDC_EDIT_RESUMETIME),!pOptions->bExeIATHooks);
    
    // wait time before injection checked
    wParam=pOptions->StartupApplicationbOnlyAfter?BST_CHECKED:BST_UNCHECKED;
    SendDlgItemMessage(mhWndDialog,IDC_CHECK_START_APP_WAIT_TIME,(UINT)BM_SETCHECK,wParam,0);

    // resume time from dialog interface
    SetDlgItemInt(mhWndDialog,IDC_EDIT_RESUMETIME,pOptions->StartupApplicationOnlyAfterMs,FALSE);

    // kill after
    wParam=pOptions->StopAndKillAfter?BST_CHECKED:BST_UNCHECKED;
    SendDlgItemMessage(mhWndDialog,IDC_CHECK_STOP_AND_KILL_APP,(UINT)BM_SETCHECK,wParam,0);
    SetDlgItemInt(mhWndDialog,IDC_EDIT_STOP_AND_KILL_APP_TIME,pOptions->StopAndKillAfterMs,FALSE);

    // wait time before injection for all processes checked
    wParam=pOptions->StartAllProcessesbOnlyAfter?BST_CHECKED:BST_UNCHECKED;
    SendDlgItemMessage(mhWndDialog,IDC_CHECK_INJECT_IN_ALL_ONLY_AFTER,(UINT)BM_SETCHECK,wParam,0);

    // wait time before injection for all processes
    SetDlgItemInt(mhWndDialog,IDC_EDIT_INSERT_AFTER,pOptions->StartAllProcessesOnlyAfterMs,FALSE);

    // set filters applies to monitoring
    wParam=pOptions->bFiltersApplyToMonitoring?BST_CHECKED:BST_UNCHECKED;
    SendDlgItemMessage(mhWndDialog,IDC_CHECK_FILTERS_APPLY_TO_MONITORING,(UINT)BM_SETCHECK,wParam,0);

    // set filters applies to faking
    wParam=pOptions->bFiltersApplyToOverriding?BST_CHECKED:BST_UNCHECKED;
    SendDlgItemMessage(mhWndDialog,IDC_CHECK_FILTERS_APPLY_TO_FAKING,(UINT)BM_SETCHECK,wParam,0);

    // set options only base module
    wParam=pOptions->bOnlyBaseModule?BST_CHECKED:BST_UNCHECKED;
    SendDlgItemMessage(mhWndDialog,IDC_CHECK_LOG_ONLY_BASEMODULE,(UINT)BM_SETCHECK,wParam,0);
    // enable or disable more specific filters
    if (!bNoGUI)
    {
        pToolbar->EnableButton(IDC_BUTTON_MODULES_FILTERS,(!pOptions->bOnlyBaseModule) && bStarted);
        EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MODULES_FILTERS),(!pOptions->bOnlyBaseModule) && bStarted);
        EnableWindow(GetDlgItem(mhWndDialog,IDC_CHECK_USE_MODULE_LIST),!pOptions->bOnlyBaseModule);
        EnableWindow(GetDlgItem(mhWndDialog,IDC_RADIO_EXCLUSION_LIST),!pOptions->bOnlyBaseModule);
        EnableWindow(GetDlgItem(mhWndDialog,IDC_RADIO_INCLUSION_LIST),!pOptions->bOnlyBaseModule);
    }

    // update use not hooked module list
    wParam=pOptions->bUseFilterModulesFileList?BST_CHECKED:BST_UNCHECKED;
    SendDlgItemMessage(mhWndDialog,IDC_CHECK_USE_MODULE_LIST,(UINT)BM_SETCHECK,wParam,0);
    if (pOptions->bFilterModulesFileListIsExclusionList)
    {
        SendDlgItemMessage(mhWndDialog,IDC_RADIO_EXCLUSION_LIST,(UINT)BM_SETCHECK,BST_CHECKED,0);
        SendDlgItemMessage(mhWndDialog,IDC_RADIO_INCLUSION_LIST,(UINT)BM_SETCHECK,BST_UNCHECKED,0);
        // update filtering module list
        SetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->ExclusionFiltersModulesFileList);
    }
    else
    {
        SendDlgItemMessage(mhWndDialog,IDC_RADIO_EXCLUSION_LIST,(UINT)BM_SETCHECK,BST_UNCHECKED,0);
        SendDlgItemMessage(mhWndDialog,IDC_RADIO_INCLUSION_LIST,(UINT)BM_SETCHECK,BST_CHECKED,0);
        // update filtering module list
        SetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->InclusionFiltersModulesFileList);
    }


    // update ExportOnlySelected state
    pExportPopUpMenu->SetCheckedState(idMenuExportOnlySelected,pOptions->ExportOnlySelected);

    // update ExportOnlyVisible state
    pExportPopUpMenu->SetCheckedState(idMenuExportOnlyVisible,pOptions->ExportOnlyVisible);

    // update ExportFullParametersContent state
    pExportPopUpMenu->SetCheckedState(idMenuExportFullParametersContent,pOptions->ExportFullParametersContent);

    // update COM hooking state
    if(pOptions->bComAutoHookingEnabled)
        pToolbar->SetButtonState(IDC_BUTTON_COM,pToolbar->GetButtonState(IDC_BUTTON_COM)|TBSTATE_CHECKED);

    // store enable .Net hooking
    if (pOptions->bNetProfilingEnabled)
        pToolbar->SetButtonState(IDC_BUTTON_NET,pToolbar->GetButtonState(IDC_BUTTON_NET)|TBSTATE_CHECKED);

    return TRUE;
}
//-----------------------------------------------------------------------------
// Name: Init
// Object: Initialize objects that requires the Dialog exists
//          and sets some dialog properties
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void Init()
{
    // set error report default parent window handle
    CAPIError::SetParentWindowHandle(mhWndDialog);

    // load app icon
    CDialogHelper::SetIcon(mhWndDialog,IDI_ICON_MAINWINDOW);
    
    // set keyboard hook for shortcuts
    hKeyboardHook  = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetCurrentThreadId());

    // set button icon
    hIconWizard=(HICON)LoadImage(mhInstance,MAKEINTRESOURCE(IDI_ICON_WIZARD),IMAGE_ICON,24,24,LR_DEFAULTCOLOR|LR_SHARED);
    SendDlgItemMessage(mhWndDialog,IDC_BUTTON_MONITORING_WIZARD,BM_SETIMAGE,IMAGE_ICON,(LPARAM)hIconWizard);

    hIconRefresh=(HICON)LoadImage(mhInstance,MAKEINTRESOURCE(IDI_ICON_REFRESH),IMAGE_ICON,24,24,LR_DEFAULTCOLOR|LR_SHARED);
    SendDlgItemMessage(mhWndDialog,IDC_BUTTON_MONITORING_RELOAD,BM_SETIMAGE,IMAGE_ICON,(LPARAM)hIconRefresh);
    SendDlgItemMessage(mhWndDialog,IDC_BUTTON_FAKING_RELOAD,BM_SETIMAGE,IMAGE_ICON,(LPARAM)hIconRefresh);
    SendDlgItemMessage(mhWndDialog,IDC_BUTTON_UPDATE_MODULE_FILTERS_LIST,BM_SETIMAGE,IMAGE_ICON,(LPARAM)hIconRefresh);

    hIconEdit=(HICON)LoadImage(mhInstance,MAKEINTRESOURCE(IDI_ICON_EDIT),IMAGE_ICON,24,24,LR_DEFAULTCOLOR|LR_SHARED);
    SendDlgItemMessage(mhWndDialog,IDC_BUTTON_EDIT_MODULE_FILTERS_LIST,BM_SETIMAGE,IMAGE_ICON,(LPARAM)hIconEdit);

    // Init log monitoring listview
    pListview=new CListview(GetDlgItem(mhWndDialog,IDC_LISTLOGMONITORING));

    // create a temporary CApiOverride object only to initialize listview
    CApiOverride pobj;
    pobj.SetMonitoringListview(pListview);
    pobj.InitializeMonitoringListview();
    pListview->SetSelectItemCallback(CallBackLogListviewItemSelection,NULL);
    pListview->EnableDefaultCustomDraw(FALSE);
    pListview-> EnableColumnReOrdering(TRUE);
    int ColumnsOrder[CApiOverride::NbColumns];
    BOOL ColumnOrderError = FALSE;
    for (int cnt=CApiOverride::FirstColumn;cnt<=CApiOverride::LastColumn;cnt++)
    {
        if (pOptions->ListviewLogsColumnsInfos[cnt].bVisible)
        {
            if (pOptions->ListviewLogsColumnsInfos[cnt].Size) // else let default size set by InitializeMonitoringListview
            {
                if (pOptions->ListviewLogsColumnsInfos[cnt].Size<5)
                    pOptions->ListviewLogsColumnsInfos[cnt].Size=5;
                pListview->SetColumnWidth(cnt,pOptions->ListviewLogsColumnsInfos[cnt].Size);
            }
        }
        else
            pListview->HideColumn(cnt);
        ColumnsOrder[cnt] = pOptions->ListviewLogsColumnsInfos[cnt].iOrder;
        if ( ( ColumnsOrder[cnt] > (int)CApiOverride::LastColumn)
             ||( ColumnsOrder[cnt] < (int)CApiOverride::FirstColumn)
           )
            ColumnOrderError = TRUE;
    }
    if (!ColumnOrderError)
        ListView_SetColumnOrderArray(pListview->GetControlHandle(),CApiOverride::NbColumns,ColumnsOrder);
    // don't show clear menu to use our one 
    // this->pListview->ShowClearPopupMenu(FALSE); // not required until clear menu is hidden by default

    // replace default listview popup menu "save selected" and "save all" 
    // by "export selected" and "export all" for better user comprehension
    TCHAR psz[MAX_PATH];
    int MenuID;
    // loop throw menu
    for (int cnt=0;cnt<pListview->pPopUpMenu->GetItemCount();cnt++)
    {
        // retrieve menu id
        MenuID=pListview->pPopUpMenu->GetID(cnt);
        if (MenuID<0)
            continue;
        // retrieve menu text
        if (pListview->pPopUpMenu->GetText(MenuID,psz,MAX_PATH)<=0)
            continue;

        // look for copy selected
        if(_tcscmp(psz,_T("Copy Selected"))==0)
        {
            pListview->pPopUpMenu->SetIcon(MenuID,IDI_ICON_CLIPBOARD_SMALL,mhInstance);
        }
        // look for copy all
        else if(_tcscmp(psz,_T("Copy All"))==0)
        {
            pListview->pPopUpMenu->SetIcon(MenuID,IDI_ICON_COPYALL_SMALL,mhInstance);
        }

        // look for save selected
        else if (_tcscmp(psz,_T("Save Selected"))==0)
        {
            // replace it by export selected
            pListview->pPopUpMenu->SetText(MenuID,_T("Export Selected"));
            pListview->pPopUpMenu->SetIcon(MenuID,IDI_ICON_EXPORT,mhInstance,16,16);
        }
        // look for save all
        else if(_tcscmp(psz,_T("Save All"))==0)
        {
            // replace it by export all
            pListview->pPopUpMenu->SetText(MenuID,_T("Export All"));
            pListview->pPopUpMenu->SetIcon(MenuID,IDI_ICON_EXPORTALL_SMALL,mhInstance);
        }
    }

    pListview->pPopUpMenu->Add(_T("Save All"),IDI_ICON_SAVE_SMALL,mhInstance,2);
    pListview->pPopUpMenu->AddSeparator();
    pListview->pPopUpMenu->Add(MENU_INVERT_SELECTION);
    pListview->pPopUpMenu->AddSeparator();
    pListview->pPopUpMenu->Add(MENU_REMOVE_SELECTED_ITEMS,IDI_ICON_REMOVE_SMALL,mhInstance);
    pListview->pPopUpMenu->Add(MENU_CLEAR,IDI_ICON_CLEAR_SMALL,mhInstance);
    pListview->pPopUpMenu->AddSeparator();
    pListview->pPopUpMenu->Add(MENU_ONLINE_MSDN_HELP,IDI_ICON_MSDN_SMALL,mhInstance);
    pListview->pPopUpMenu->Add(MENU_GOOGLE_FUNC_NAME,IDI_ICON_GOOGLE_SMALL,mhInstance);
    pListview->pPopUpMenu->Add(MENU_COPY_FUNC_NAME,IDI_ICON_COPY_SMALL,mhInstance);
    pListview->pPopUpMenu->AddSeparator();
    pListview->pPopUpMenu->Add(MENU_GOTO_PREVIOUS_FAILURE,IDI_ICON_UP,mhInstance);
    pListview->pPopUpMenu->Add(MENU_GOTO_NEXT_FAILURE,IDI_ICON_DOWN,mhInstance);
    pListview->pPopUpMenu->AddSeparator();

    pListview->pPopUpMenu->Add(MENU_GET_NONREBASED_VIRTUAL_ADDRESS);
    pListview->pPopUpMenu->Add(MENU_SHOW_GETLASTERROR_MESSAGE,IDI_ICON_ERROR_SMALL,mhInstance);
    pListview->pPopUpMenu->Add(MENU_SHOW_RETURN_VALUE_ERROR_MESSAGE,IDI_ICON_RETURN_ERROR_SMALL,mhInstance);
    pListview->SetPopUpMenuItemClickCallback((CListview::tagPopUpMenuItemClickCallback)ListviewPopUpMenuItemClickCallback,pListview);


    // Init details listview
    pListviewDetails=new CListview(GetDlgItem(mhWndDialog,IDC_LISTDETAILS));
    pListviewDetails->SetStyle(TRUE,FALSE,FALSE,FALSE);
    pListviewDetails->EnableDefaultCustomDraw(FALSE);
    pListviewDetails->EnableColumnSorting(FALSE);
    pListviewDetails->AddColumn(_T("Property"),120,LVCFMT_LEFT);
    pListviewDetails->AddColumn(_T("Value"),880,LVCFMT_LEFT);

    pListviewDetails->pPopUpMenu->AddSeparator();
    ListviewDetailsMenuIDHexDisplay=pListviewDetails->pPopUpMenu->Add(MENU_DISPLAY_DATA_IN_HEX_WINDOW);
    pListviewDetails->SetPopUpMenuItemClickCallback(ListviewDetailsPopUpMenuItemClickCallback,pListviewDetails);
    pListviewDetails->SetSelectItemCallback(ListviewDetailsSelectItemCallback,NULL);

    // init type display list view
    pListviewDetailsTypesDisplay = new CListview(GetDlgItem(mhWndDialog,IDC_LISTDETAILSTYPESDISPLAY));
    pListviewDetailsTypesDisplay->SetStyle(TRUE,FALSE,FALSE,FALSE);
    pListviewDetailsTypesDisplay->EnableDefaultCustomDraw(FALSE);
    pListviewDetailsTypesDisplay->EnableColumnSorting(FALSE);
    pListviewDetailsTypesDisplay->AddColumn(_T("Representation"),120,LVCFMT_LEFT);
    pListviewDetailsTypesDisplay->AddColumn(_T("Value"),420,LVCFMT_LEFT);
    
    // Init monitoring files listview
    pListViewMonitoringFiles=new CListview(GetDlgItem(mhWndDialog,IDC_LIST_MONITORING_FILES));
    pListViewMonitoringFiles->SetStyle(TRUE,FALSE,FALSE,FALSE);
    pListViewMonitoringFiles->AddColumn(_T("Name"),200,LVCFMT_LEFT);
    pListViewMonitoringFiles->AddColumn(_T("Path"),200,LVCFMT_LEFT);
    pListViewMonitoringFiles->EnablePopUpMenu(FALSE);
    pListViewMonitoringFiles->EnableColumnSorting(FALSE);

    // Init overriding files listview
    pListViewOverridingFiles=new CListview(GetDlgItem(mhWndDialog,IDC_LIST_FAKING_FILES));
    pListViewOverridingFiles->SetStyle(TRUE,FALSE,FALSE,FALSE);
    pListViewOverridingFiles->AddColumn(_T("Name"),200,LVCFMT_LEFT);
    pListViewOverridingFiles->AddColumn(_T("Path"),200,LVCFMT_LEFT);
    pListViewOverridingFiles->EnablePopUpMenu(FALSE);
    pListViewOverridingFiles->EnableColumnSorting(FALSE);

    // init toolbar
    pToolbar=new CToolbar(mhInstance,mhWndDialog,TRUE,TRUE,24,24);
    pToolbar->AddButton(IDC_BUTTON_START_STOP_HOOKS,IDI_ICON_START,IDI_ICON_START,IDI_ICON_START_HOT,_T("Start"));
    // add start/stop monitoring button in disabled pause state
    pToolbar->AddButton(IDC_BUTTON_START_STOP_MONITORING,IDI_ICON_PAUSE_MONITORING,IDI_ICON_PAUSE_MONITORING_DISABLE,IDI_ICON_PAUSE_MONITORING_HOT,_T("Pause Monitoring"));
    pToolbar->EnableButton(IDC_BUTTON_START_STOP_MONITORING,FALSE);
    // add start/stop faking button in disabled pause state
    pToolbar->AddButton(IDC_BUTTON_START_STOP_FAKING,IDI_ICON_PAUSE_FAKING,IDI_ICON_PAUSE_FAKING_DISABLED,IDI_ICON_PAUSE_FAKING_HOT,_T("Pause Faking"));
    pToolbar->EnableButton(IDC_BUTTON_START_STOP_FAKING,FALSE);

    // COM auto hooking
    pToolbar->AddCheckButton(IDC_BUTTON_COM,IDI_ICON_COM,_T("Enable COM Auto-Hooking"));
    // enable .NET hooking
    pToolbar->AddCheckButton(IDC_BUTTON_NET,IDI_ICON_NET,_T("Enable .Net Hooking (Applies only to new started applications and services)"));

    // separator
    pToolbar->AddSeparator();
    // process filters
    pToolbar->AddButton(IDC_BUTTON_PROCESS_FILTERS,IDI_ICON_PROCESSES_FILTERS,IDI_ICON_PROCESSES_FILTERS_DISABLED,_T("Processes Filters"));
    pToolbar->EnableButton(IDC_BUTTON_PROCESS_FILTERS,FALSE);
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_PROCESS_FILTERS),FALSE);

    // modules filters
    pToolbar->AddButton(IDC_BUTTON_MODULES_FILTERS,IDI_ICON_MODULES_FILTERS,IDI_ICON_MODULES_FILTERS_DISABLED,_T("Modules Filters"));
    pToolbar->EnableButton(IDC_BUTTON_MODULES_FILTERS,FALSE);
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MODULES_FILTERS),FALSE);

    // separator
    pToolbar->AddSeparator();
    // clear list
    pToolbar->AddButton(IDC_BUTTON_CLEAR_MONITORING_LISTVIEW,IDI_ICON_CLEAR,_T("Clear Monitoring List"));
    // load
    pToolbar->AddButton(IDC_BUTTON_LOAD_LOGS,IDI_ICON_OPEN,IDI_ICON_OPEN_DISABLED,_T("Open Logs"));
    // save 
    pToolbar->AddButton(IDC_BUTTON_SAVE_LOGS,IDI_ICON_SAVE,_T("Save Logs"));
    // exports
    pExportPopUpMenu=new CPopUpMenu();
    idMenuExportHTML=pExportPopUpMenu->Add(_T("Html"));
    idMenuExportXML=pExportPopUpMenu->Add(_T("Xml"));
    idMenuExportTXT=pExportPopUpMenu->Add(_T("Txt"));
    idMenuExportCSV=pExportPopUpMenu->Add(_T("Csv"));
    pExportPopUpMenu->AddSeparator();
    idMenuExportOnlySelected=pExportPopUpMenu->Add(_T("Only Selected"));
    idMenuExportOnlyVisible=pExportPopUpMenu->Add(_T("Only Visible Columns"));
    idMenuExportFullParametersContent=pExportPopUpMenu->Add(_T("Full Parameters Content"));
    
    pToolbar->SetDropDownMenuCallBack(CallBackToolBarDropDownMenu,NULL);

    pToolbar->AddDropDownButton(IDC_BUTTON_EXPORT_LOGS,IDI_ICON_EXPORT,_T("Export Logs"),pExportPopUpMenu,TRUE);

    // separator
    pToolbar->AddSeparator();

    // memory
    pToolbar->AddButton(IDC_BUTTON_MEMORY,IDI_ICON_MEMORY,_T("Processes Memory Operations"));

    // dump
    pToolbar->AddButton(IDC_BUTTON_DUMP,IDI_ICON_DUMP,IDI_ICON_DUMP_DISABLED,_T("Dump Hooked Processes"));
    pToolbar->EnableButton(IDC_BUTTON_DUMP,FALSE);

    // Internal Call
    pToolbar->AddButton(IDC_BUTTON_INTERNAL_CALL,IDI_ICON_REMOTE_CALL,IDI_ICON_REMOTE_CALL_DISABLED,_T("Make Call Inside Hooked Process"));
    pToolbar->EnableButton(IDC_BUTTON_INTERNAL_CALL,FALSE);

    // COM tools
    pCOMToolsPopUpMenu=new CPopUpMenu();
    idMenuComToolsMonitoringInterfaceWizard=pCOMToolsPopUpMenu->Add(_T("Monitoring Interface Wizard"));
    pCOMToolsPopUpMenu->AddSeparator();
    idMenuComToolsHookedComObjectInteraction=pCOMToolsPopUpMenu->Add(_T("Hooked Com Object Interaction"));
    pCOMToolsPopUpMenu->SetEnabledState(idMenuComToolsHookedComObjectInteraction,FALSE);
    idMenuComToolsShowMethodsAddressInModule=pCOMToolsPopUpMenu->Add(_T("Show Methods Addresses"));
    pCOMToolsPopUpMenu->AddSeparator();
    idMenuComToolsShowAllCLSID=pCOMToolsPopUpMenu->Add(_T("Show Computer Known CLSID"));
    idMenuComToolsNameCLSIDConvert=pCOMToolsPopUpMenu->Add(_T("CLSID <-> ProgId"));
    idMenuComToolsNameIIDConvert=pCOMToolsPopUpMenu->Add(_T("IID <-> Interface Name"));
    pToolbar->AddDropDownButton(IDC_BUTTON_COM_TOOLS,IDI_ICON_COM_TOOLS,_T("COM Tools"),pCOMToolsPopUpMenu,TRUE);

    // NET tools
    pToolbar->AddButton(IDC_BUTTON_NET_INTERACTION,IDI_ICON_NET_INTERACTION,_T(".Net Jitted Functions Interaction"));
    
    // Plugins
    pPluginsPopUpMenu =new CPopUpMenu();
    pToolbar->AddDropDownButton(IDC_BUTTON_PLUGINS,IDI_ICON_PLUGINS,_T("Plugins"),pPluginsPopUpMenu,TRUE);

    // separator
    pToolbar->AddSeparator();

    // search
    pToolbar->AddButton(IDC_BUTTON_SEARCH_IN_LOGS,IDI_ICON_SEARCH,_T("Search"));
    // compare
    pToolbar->AddButton(IDC_BUTTON_COMPARE_LOGS,IDI_ICON_COMPARE,_T("Compare Two First Selected Logs"));
    // stats
    pToolbar->AddButton(IDC_BUTTON_STATS,IDI_ICON_STATS,_T("Stats"));
    // analysis
    pToolbar->AddButton(IDC_BUTTON_ANALYSIS,NULL,BTNS_CHECK|BTNS_AUTOSIZE,TBSTATE_ENABLED,IDI_ICON_ANALYSIS,IDI_ICON_ANALYSIS_DISABLED,IDI_ICON_ANALYSIS,_T("Call Analysis"));

    // separator
    pToolbar->AddSeparator();

    // error messages
    pErrorMessagePopUpMenu=new CPopUpMenu();
    idMenuErrorMessageGetLastError=pErrorMessagePopUpMenu->Add(_T("Show GetLastError Message Description"));
    idMenuErrorMessageReturnedValue=pErrorMessagePopUpMenu->Add(_T("Show Returned Value Message Description"));
    pToolbar->AddDropDownButton(IDC_BUTTON_LAST_ERROR_MESSAGE,IDI_ICON_ERROR_MESSAGE,_T("Show Error Message Description"),pErrorMessagePopUpMenu,FALSE);
    
    // msdn
    pToolbar->AddButton(IDC_BUTTON_MSDN,IDI_ICON_MSDN,_T("Online MSDN"));
    // string convert
    pToolbar->AddButton(IDC_BUTTON_CONVERT,IDI_ICON_CONVERT,_T("String <-> Hexa Convert"));

    // separator
    pToolbar->AddSeparator();
    // exports
    pToolbar->AddButton(IDC_BUTTON_SELECT_COLUMNS,IDI_ICON_COLUMNS,_T("Select Columns"));
    // options
    pToolbar->AddButton(IDC_BUTTON_OPTIONS,IDI_ICON_OPTIONS,_T("Options"));

    // separator
    pToolbar->AddSeparator();
    // Monitoring Files Generator
    pToolbar->AddButton(IDC_BUTTON_MONITORING_FILES_GENERATOR,IDI_ICON_MONITORING_BUILDER,_T("Monitoring File Builder"));

    // separator
    pToolbar->AddSeparator();
    // Check for update
    pToolbar->AddButton(IDC_BUTTON_CHECK_FOR_UPDATE,IDI_ICON_CHECK_FOR_UPDATE,_T("Check For Update"));
    // Bug Report
    pToolbar->AddButton(IDC_BUTTON_BUG_REPORT,IDI_ICON_REPORT_BUG,_T("Report Bug"));

    // separator
    pToolbar->AddSeparator();
    // help
    pToolbar->AddButton(IDC_BUTTON_HELP,IDI_ICON_HELP,_T("WinAPIOverride Help"));
    // about
    pToolbar->AddButton(IDC_BUTTON_ABOUT,IDI_ICON_ABOUT,_T("About"));

    // separator
    pToolbar->AddSeparator();
    // Donation
    pToolbar->AddButton(IDC_BUTTON_DONATION,IDI_ICON_PAYPAL,_T("Make a Donation"));


    // create rebar
    pRebar=new CRebar(mhInstance,mhWndDialog);
    pRebar->AddToolBarBand(pToolbar->GetControlHandle(),NULL,FALSE,FALSE,TRUE);

    // splitter for loading and faking monitoring
    pSplitterLoadMonitoringAndFaking=new CSplitter(mhInstance,mhWndDialog,FALSE,FALSE,TRUE,FALSE,40,
                                                    IDI_ICON_UP,IDI_ICON_UP_HOT,IDI_ICON_UP_DOWN,
                                                    IDI_ICON_DOWN,IDI_ICON_DOWN_HOT,IDI_ICON_DOWN_DOWN,
                                                    16,16);

    pSplitterLoadMonitoringAndFaking->SetMoveCallBack(SplitterLoadMonitoringAndFakingMove,NULL);

    RECT Rect;
    RECT RectWindow;
    GetWindowRect(GetDlgItem(mhWndDialog,IDC_STATICPIDSELECT),&Rect);
    GetWindowRect(mhWndDialog,&RectWindow);
    Rect.top-=RectWindow.top;
    pSplitterLoadMonitoringAndFaking->TopMinFreeSpace=Rect.top;
    pSplitterLoadMonitoringAndFaking->BottomMinFreeSpace=100;
    pSplitterLoadMonitoringAndFaking->LeftMinFreeSpace=5;
    pSplitterLoadMonitoringAndFaking->RightMinFreeSpace=5;

    // splitter for details
    pSplitterDetails=new CSplitter(mhInstance,mhWndDialog,TRUE,TRUE,FALSE,TRUE,40,
                                    IDI_ICON_RIGHT,IDI_ICON_RIGHT_HOT,IDI_ICON_RIGHT_DOWN,
                                    IDI_ICON_LEFT,IDI_ICON_LEFT_HOT,IDI_ICON_LEFT_DOWN,
                                    16,16);

    pSplitterDetails->SetMoveCallBack(SplitterDetailsMove,NULL);

    GetWindowRect(GetDlgItem(mhWndDialog,IDC_LISTLOGMONITORING),&Rect);
    pSplitterDetails->TopMinFreeSpace=Rect.top-RectWindow.top;
    pSplitterDetails->BottomMinFreeSpace=RectWindow.bottom-Rect.bottom-RectWindow.top;
    pSplitterDetails->RightMinFreeSpace=SPACE_BETWEEN_CONTROLS;


    // splitter for config
    pSplitterConfig=new CSplitter(mhInstance,mhWndDialog,TRUE,FALSE,TRUE,FALSE,100,
                                    IDI_ICON_UP,IDI_ICON_UP_HOT,IDI_ICON_UP_DOWN,
                                    IDI_ICON_DOWN,IDI_ICON_DOWN_HOT,IDI_ICON_DOWN_DOWN,
                                    16,16);

    pSplitterConfig->SetCollapsedStateChangeCallBack(CallBackSplitterConfigCollapsedStateChange,NULL);

    GetWindowRect(GetDlgItem(mhWndDialog,IDC_STATICPIDSELECT),&Rect);
    GetWindowRect(mhWndDialog,&RectWindow);
    Rect.top-=RectWindow.top;
    pSplitterConfig->TopMinFreeSpace=Rect.top;
    GetWindowRect(GetDlgItem(mhWndDialog,IDC_STATIC_MODULES_FILTERS),&Rect);
    pSplitterConfig->BottomMinFreeSpace=RectWindow.bottom-Rect.bottom;
    pSplitterConfig->LeftMinFreeSpace=5;
    pSplitterConfig->RightMinFreeSpace=5;

    pSplitterConfig->AllowSizing=FALSE;

    // ToolTips
    pToolTipEditModulesExclusionFilters=
        new CToolTip(GetDlgItem(mhWndDialog,IDC_BUTTON_EDIT_MODULE_FILTERS_LIST),
                     _T("Edit exclusion list"),
                     TRUE);
    pToolTipReloadModulesExclusionFilters=
        new CToolTip(GetDlgItem(mhWndDialog,IDC_BUTTON_UPDATE_MODULE_FILTERS_LIST),
                     _T("Reload exclusion list"),
                     TRUE);
    pToolTipMonitoringWizard=
        new CToolTip(GetDlgItem(mhWndDialog,IDC_BUTTON_MONITORING_WIZARD),
                     _T("Monitoring Wizard"),
                     TRUE);
    pToolTipMonitoringReloadLastSession=
        new CToolTip(GetDlgItem(mhWndDialog,IDC_BUTTON_MONITORING_RELOAD),
                     _T("Reload monitoring files"),
                     TRUE);
    pToolTipFakingReloadLastSession=
        new CToolTip(GetDlgItem(mhWndDialog,IDC_BUTTON_FAKING_RELOAD),
                     _T("Reload overriding dlls"),
                     TRUE);



    // disable monitoring faking interface
    EnableMonitoringFakingInterface(FALSE);

    // init select window
    pSelectWindow->Initialize(mhInstance,mhWndDialog,IDC_STATIC_PICTURE_SELECT_TARGET,
                            IDB_BLANK,IDB_CROSS,IDC_CURSOR_TARGET);

    // refresh user interface after having created all components
    Resize(bUserInterfaceInStartedMode);

    // set user interface MUST BE CALLED ONLY AFTER HAVING CREATED ALL COMPONENTS
    SetGUIFromOptions();

    // accept drag and drop operation
    DragAcceptFiles(mhWndDialog, TRUE);

    // add autocomplete on path edit
    SHAutoComplete(GetDlgItem(mhWndDialog,IDC_EDIT_APP_PATH),SHACF_FILESYSTEM | SHACF_URLALL | SHACF_USETAB);
    SHAutoComplete(GetDlgItem(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST),SHACF_FILESYSTEM | SHACF_URLALL | SHACF_USETAB);
    SHAutoComplete(GetDlgItem(mhWndDialog,IDC_EDIT_FAKING_FILE),SHACF_FILESYSTEM | SHACF_URLALL | SHACF_USETAB);
    SHAutoComplete(GetDlgItem(mhWndDialog,IDC_EDIT_MONITORING_FILE),SHACF_FILESYSTEM | SHACF_URLALL | SHACF_USETAB);

    // if command line start the hooks MUST BE AT THE END OF DIALOG INIT
    if (bCommandLine)
        ThreadedStartStop();


    // start user define type changes watching
    TCHAR PathToWatch[MAX_PATH];
    _tcscpy(PathToWatch,pszApplicationPath);
    _tcscat(PathToWatch,APIOVERRIDE_USER_TYPES_PATH);
    ShellWatcher.StartWatching(PathToWatch,
                                mhWndDialog,
                                WM_SHELL_CHANGE_NOTIFY,
                                SHCNE_ATTRIBUTES | SHCNE_DELETE | SHCNE_RENAMEFOLDER | SHCNE_RENAMEITEM | SHCNE_RMDIR | SHCNE_UPDATEDIR | SHCNE_MKDIR | SHCNE_UPDATEITEM | SHCNE_CREATE,// SHCNE_ALLEVENTS,
                                TRUE);

    pPluginManager->SetDialog(mhWndDialog);
    pPluginManager->LoadAndInitialize();
}

//-----------------------------------------------------------------------------
// Name: FreeMemoryAllocatedByCommandLineParsing
// Object: free memory allocated by the ParseCommandLine function
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void FreeMemoryAllocatedByCommandLineParsing()
{
    if (CommandLineOverridingDllArray)
    {
        CMultipleElementsParsing::ParseStringArrayFree(CommandLineOverridingDllArray,CommandLineOverridingDllArraySize);
        CommandLineOverridingDllArray=NULL;
        CommandLineOverridingDllArraySize=0;
    }
    if (CommandLineMonitoringFilesArray)
    {
        CMultipleElementsParsing::ParseStringArrayFree(CommandLineMonitoringFilesArray,CommandLineMonitoringFilesArraySize);
        CommandLineMonitoringFilesArray=NULL;
        CommandLineMonitoringFilesArraySize=0;
    }
}


//-----------------------------------------------------------------------------
// Name: LoadCommandLineMonitoringAndFakingFiles
// Object: load monitoring files and faking dll provide in arg of command line
//          must be called only once if not bNoGui as it add files to listviews
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void LoadCommandLineMonitoringAndFakingFiles()
{
    bLoadCommandLineMonitoringAndFakingFilesNeverCalled=FALSE;

    DWORD dwCnt;
    // load monitoring files
    for (dwCnt=0;dwCnt<CommandLineMonitoringFilesArraySize;dwCnt++)
    {
        LoadMonitoringFile(CommandLineMonitoringFilesArray[dwCnt]);
    }
    // load overriding dlls
    for (dwCnt=0;dwCnt<CommandLineOverridingDllArraySize;dwCnt++)
    {
        LoadOverridingFile(CommandLineOverridingDllArray[dwCnt]);
    }
}

//-----------------------------------------------------------------------------
// Name: OnMouseDown
// Object: WM_MOUSEDOWN callback
// Parameters :
//     in  : LPARAM lParam : Dialog callback LPARAM
//     out :
//     return : 
//-----------------------------------------------------------------------------
void OnMouseDown(LPARAM lParam)
{
    pSelectWindow->MouseDown(lParam);
}
//-----------------------------------------------------------------------------
// Name: OnMouseUp
// Object: WM_MOUSEUP callback
// Parameters :
//     in  : LPARAM lParam : Dialog callback LPARAM
//     out :
//     return : 
//-----------------------------------------------------------------------------
void OnMouseUp(LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    pSelectWindow->MouseUp();
}

//-----------------------------------------------------------------------------
// Name: OnMouseMove
// Object: WM_MOUSEMOVE callback
// Parameters :
//     in  : LPARAM lParam : Dialog callback LPARAM
//     out :
//     return : 
//-----------------------------------------------------------------------------
void OnMouseMove(LPARAM lParam)
{
    BOOL bWindowChanged=FALSE;
    pSelectWindow->MouseMove(lParam,&bWindowChanged);
    if (bWindowChanged)
    {
        // set process ID in IDC_EDIT_PID
        bIDC_EDIT_PIDChangedInternally=TRUE;
        SetDlgItemInt(mhWndDialog,IDC_EDIT_PID,pSelectWindow->WindowProcessID,FALSE);
        bIDC_EDIT_PIDChangedInternally=FALSE;
    }
}



DWORD WINAPI StopHookingAfterTimer(LPVOID lpParameter)
{
    // if process has been created, query to kill it
    STOP_AFTER_TIMER_INFOS* pStopAfterTimerInfos = (STOP_AFTER_TIMER_INFOS*)lpParameter;
    DWORD dwRet;

    HANDLE ExitEvents[]={hevtUnexpectedUnload,evtStopping,evtApplicationExit};
    dwRet = ::WaitForMultipleObjects(_countof(ExitEvents),ExitEvents,FALSE,pStopAfterTimerInfos->StopTimerValue);
    switch(dwRet)
    {
    case WAIT_TIMEOUT:
        if (bStarted)// should be always TRUE
            StartStop();
        ::TerminateProcess(pStopAfterTimerInfos->hProcess,0);
        if (evtNoGuiTargetStoppedByStopAndKillAfter)
            SetEvent(evtNoGuiTargetStoppedByStopAndKillAfter);
        // sleep a little after terminating process, allowing the pStopAfterTimerInfos->hProcess event
        // to be processed by all waiter thread.
        // Since handle closing is not an urgent task, and all used next operations are using allocated
        // memory used only by current thread there's no time limit
        ::Sleep(5000);
        break;
    }
    ::CloseHandle(pStopAfterTimerInfos->hProcess);
    delete pStopAfterTimerInfos;
    return dwRet;
}

void SetStartStopButtonState(BOOL bStateStarted)
{
    if (bStateStarted)
    {
        // put IDC_BUTTON_START_STOP_HOOKS caption to Stop
        pToolbar->ReplaceToolTipText(IDC_BUTTON_START_STOP_HOOKS,_T("Stop"));
        pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_HOOKS,CToolbar::ImageListTypeEnable,IDI_ICON_STOP);
        pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_HOOKS,CToolbar::ImageListTypeHot,IDI_ICON_STOP_HOT);
    }
    else
    {
        // put IDC_BUTTON_START_STOP_HOOKS caption to Start
        pToolbar->ReplaceToolTipText(IDC_BUTTON_START_STOP_HOOKS,_T("Start"));
        pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_HOOKS,CToolbar::ImageListTypeEnable,IDI_ICON_START);
        pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_HOOKS,CToolbar::ImageListTypeHot,IDI_ICON_START_HOT);
    }
}


BOOL DestroyApiOverride(CApiOverride* pApiOverride,BOOL bUserManagesLock)
{
    // remove item from link list
    if (pApiOverrideList->RemoveItemFromItemData(pApiOverride,bUserManagesLock))
    {
        // delete CApiOverride object
        delete pApiOverride;
    }

    return TRUE;
}

BOOL DestroyApiOverride(CApiOverride* pApiOverride)
{
    return DestroyApiOverride(pApiOverride,FALSE);
}

void ReportHookedProcess(CApiOverride* pApiOverride,DWORD ProcessId,BOOL bSuccess)
{
    TCHAR psz[MAX_PATH];
    TCHAR ProcName[MAX_PATH];
    if (bSuccess)
    {
        // get proc name
        *ProcName=0;
        pApiOverride->GetProcessName(ProcName,MAX_PATH);
        if (*ProcName)
            _sntprintf(psz,MAX_PATH,_T("Process 0x%X (%s) successfully hooked"),ProcessId,ProcName);
        else
            _sntprintf(psz,MAX_PATH,_T("Process 0x%X successfully hooked"),ProcessId);
        // add info to listview
        ReportMessage(pApiOverride,psz,MSG_INFORMATION);
    }
    else
    {
        // get proc name
        if (CProcessName::GetProcessName(ProcessId,ProcName))
            _sntprintf(psz,MAX_PATH,_T("Error hooking process 0x%X(%s)"),ProcessId,ProcName);
        else
            _sntprintf(psz,MAX_PATH,_T("Error hooking process 0x%X") ,ProcessId);
        // add info to listview
        ReportMessage(pApiOverride,psz,MSG_ERROR);
    }
}

void ReportUnhookedProcess(CApiOverride* pApiOverride,BOOL bSuccess)
{
    TCHAR psz[MAX_PATH];
    DWORD dwProcessID;
    TCHAR ProcName[MAX_PATH];

    // retrieve proc ID
    dwProcessID=pApiOverride->GetProcessID();  

    if (bSuccess)
    {
        *ProcName=0;
        pApiOverride->GetProcessName(ProcName,MAX_PATH);
        // try to get process name
        if (*ProcName)
            _sntprintf(psz,MAX_PATH,_T("Process 0x") _T("%X") _T(" (%s) successfully unhooked"),dwProcessID,ProcName);
        else
            _sntprintf(psz,MAX_PATH,_T("Process 0x") _T("%X") _T(" successfully unhooked"),dwProcessID);
        // add info to listview
        ReportMessage(pApiOverride,psz,MSG_INFORMATION);

    }
    else
    {
        // try to get process name
        if (CProcessName::GetProcessName(dwProcessID,ProcName))
            _sntprintf(psz,MAX_PATH,_T("Error unhooking process 0x") _T("%X") _T(" (%s)"),dwProcessID,ProcName);
        else
            _sntprintf(psz,MAX_PATH,_T("Error unhooking process 0x") _T("%X") ,dwProcessID);
        // add info to listview
        ReportMessage(pApiOverride,psz,MSG_ERROR);
    }
}

BOOL StopApiOverride(CApiOverride* pApiOverride)
{
    BOOL bSuccess = pApiOverride->Stop();
    ReportUnhookedProcess(pApiOverride,bSuccess);

    if (bSuccess)
        pPluginManager->OnProcessUnhooked(pApiOverride);
    return bSuccess;
}


//-----------------------------------------------------------------------------
// Name: ThreadedStartStop
// Object: calls StartStop in a thread
// Parameters :
//     in  : 
//     out :
//     return : TRUE on Success, FALSE in case of error
//-----------------------------------------------------------------------------
void ThreadedStartStop()
{
    CloseHandle(CreateThread(0,0,StartStopThreadStartRoutine,0,0,0));
}
DWORD WINAPI StartStopThreadStartRoutine(LPVOID lpParameter)
{
    UNREFERENCED_PARAMETER(lpParameter);
    return (DWORD)StartStop();
}

//-----------------------------------------------------------------------------
// Name: StartStop (must not be called from WndProc)
// Object: Start/Stop button callback. Start or stop hooking depending options
// Parameters :
//     in  : 
//     out :
//     return : TRUE on Success, FALSE in case of error
//-----------------------------------------------------------------------------
BOOL StartStop(BOOL Start)
{
    if ((!bNoGUI)&&pToolbar)
        // disable StartStop button
        pToolbar->EnableButton(IDC_BUTTON_START_STOP_HOOKS,FALSE);

    WaitForSingleObject(evtStartStopUnlocked,INFINITE);

    if (!bNoGUI)
        // retrieve options from user interface
        GetOptionsFromGUI();
    
    if (!Start)// Stop action
    {
        HANDLE hStopThread=CreateThread(NULL,0,Stop,NULL,0,NULL);
        // create a new thread to allow unload of monitoring/overriding of DlgProc
        WaitForSingleObject(hStopThread,INFINITE);
        CloseHandle(hStopThread);
        SetEvent(evtStartStopUnlocked);
        return TRUE;
    }

    ////////////////////////
    // else : Start action
    ////////////////////////

    BOOL bSuccess=FALSE;
    DWORD cnt;
    TCHAR psz[MAX_PATH];
    CApiOverride* pApiOverride;
    BOOL bStartSuccess;
    BOOL bUnexpectedUnload = FALSE;
    ::ResetEvent(hevtUnexpectedUnload);

    // if we want to hook all processes
    if (pOptions->StartWay==COptions::START_WAY_ALL_PROCESSES)
    {
        // start the procmon driver
        bSuccess=pProcMonInterface->StartDriver();
        if (bSuccess)
        {
            // set a callback for process creation
            pProcMonInterface->SetProcessStartCallBack(CallBackProcessCreation);
            // start monitoring
            pProcMonInterface->StartMonitoring();

            // retrieve wanted running application array
            for(cnt=0;cnt<pFilters->pdwFiltersCurrentPorcessIDSize;cnt++)
            {
                // if process is no more alive
                if (!CProcessHelper::IsAlive(pFilters->pdwFiltersCurrentPorcessID[cnt]))
                {
                        // add information to listview
                    _sntprintf(psz,MAX_PATH,_T("Process 0x") _T("%X") _T(" don't exists anymore, so it won't be hooked"),pFilters->pdwFiltersCurrentPorcessID[cnt]);
                    ReportMessage(psz,MSG_WARNING);
                    continue;
                }
                // create a new CApiOverride object
                pApiOverride=new CApiOverride(mhWndDialog);

                // set options
                SetBeforeStartOptions(pApiOverride);

                // start hooking
                bStartSuccess = pApiOverride->Start(pFilters->pdwFiltersCurrentPorcessID[cnt]);
 
                if (bStartSuccess)
                {
                    SetAfterStartOptions(pApiOverride);
                    // do report after setting options
                    ReportHookedProcess(pApiOverride,pFilters->pdwFiltersCurrentPorcessID[cnt],bStartSuccess);

                    pPluginManager->OnProcessHooked(pApiOverride);
                }
                else
                {
                    // do report before deleting
                    ReportHookedProcess(pApiOverride,pFilters->pdwFiltersCurrentPorcessID[cnt],bStartSuccess);

                    // delete pApiOverride object
                    DestroyApiOverride(pApiOverride);
                }
            }
            if (!bNoGUI)
                // enable monitoring faking interface
                EnableMonitoringFakingInterface(TRUE);

            bStarted=TRUE;
        }
    }
    else
    {
        // only one pApiOverride creation is required

        // create a new CApiOverride object
        pApiOverride=new CApiOverride(mhWndDialog);

        // set options
        SetBeforeStartOptions(pApiOverride);

        // if we want to hook application with the specified PID
        if (pOptions->StartWay==COptions::START_WAY_PID)
        {
            DWORD dwPID=pOptions->StartupApplicationPID;
            bSuccess=(dwPID!=0);

            if (bSuccess)
                // check that process is still alive
                bSuccess=CProcessHelper::IsAlive(dwPID);
            // if good pid value
            if (bSuccess)
            {
                bSuccess=pApiOverride->Start(dwPID);
                bStarted=bSuccess;
            }
            else
                MessageBox(mhWndDialog,_T("Bad Process Id number"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);

            // if pApiOverride->Start successful
            if (bSuccess)
            {
                SetAfterStartOptions(pApiOverride);

                ReportHookedProcess(pApiOverride,dwPID,bSuccess);

                pPluginManager->OnProcessHooked(pApiOverride);

                bStarted=TRUE;// fill flag now in case application crash during display of message

                if (!bNoGUI)
                    // enable monitoring faking interface
                    EnableMonitoringFakingInterface(TRUE);

                // show an information message
                if (!bCommandLine)
                    MessageBox(mhWndDialog,_T("Application Ready to be Hooked\r\nYou can now load Monitoring and Overriding files"),_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);

            }
        }
        // if we want to hook the specified app at startup
        else if (pOptions->StartWay==COptions::START_WAY_LAUNCH_APPLICATION)
        {
            PROCESS_INFORMATION ProcessInformation={0};
            PROCESS_INFORMATION* pProcessInformation;
            if (pOptions->StopAndKillAfter)
                pProcessInformation = &ProcessInformation;
            else 
                pProcessInformation = NULL;

            // if exe iat hooking is enabled
            if (pOptions->bExeIATHooks)
            {
                bSuccess=pApiOverride->Start(pOptions->StartupApplicationPath,pOptions->StartupApplicationCmd,CallBackBeforeAppResume,pApiOverride,CApiOverride::StartWayIAT,0,NULL,pProcessInformation);
                bStarted=bSuccess;
            }
            // if wait time before injection checked
            else if (pOptions->StartupApplicationbOnlyAfter)
            {
                // if good resume time value
                if (pOptions->StartupApplicationOnlyAfterMs)
                {
                    // start delay hooking
                    bSuccess=pApiOverride->Start(pOptions->StartupApplicationPath,pOptions->StartupApplicationCmd,CallBackBeforeAppResume,pApiOverride,CApiOverride::StartWaySleep,pOptions->StartupApplicationOnlyAfterMs,NULL,pProcessInformation);
                    bStarted=bSuccess;
                }
                else
                    MessageBox(mhWndDialog,_T("Bad resume time value"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
            }
            // suspended injection
            else
            {
                // start with infos
                bSuccess=pApiOverride->Start(pOptions->StartupApplicationPath,pOptions->StartupApplicationCmd,CallBackBeforeAppResume,pApiOverride,CApiOverride::StartWaySuspended,0,NULL,pProcessInformation);
                bStarted=bSuccess;
            }

            // on success
            if (bSuccess)
            {
                // if stop after timer asked
                if (pOptions->StopAndKillAfter)
                {
                    STOP_AFTER_TIMER_INFOS* pStopAfterTimerInfos = new STOP_AFTER_TIMER_INFOS;
                    // if ProcessInformation.dwProcessId == 0 (process not created by winapioverride) process won't be killed
                    pStopAfterTimerInfos->hProcess = ProcessInformation.hProcess;
                    pStopAfterTimerInfos->StopTimerValue = pOptions->StopAndKillAfterMs;
                    ::CloseHandle(::CreateThread(0,0,StopHookingAfterTimer,pStopAfterTimerInfos,0,0));

                    CloseHandle(ProcessInformation.hThread);
                    // CloseHandle(ProcessInformation.hProcess); // don't close process now as handle will be used to terminate process
                }
            }
        }
        
        // bUnexpectedUnload = TRUE in case of application crash at startup
        bUnexpectedUnload = ( ::WaitForSingleObject(hevtUnexpectedUnload,0) == WAIT_OBJECT_0 );
        
        // on error
        if (!bSuccess)
        {
            // if hevtUnexpectedUnload has occurred "DestroyApiOverride(pApiOverride);" has already been called
            if (!bUnexpectedUnload)
            {
                // free memory 
                DestroyApiOverride(pApiOverride);
            }
        }
    }

    if (bSuccess)
    {
        if ( (!bNoGUI) && pToolbar && bStarted && (!bUnexpectedUnload) )
        {
            SetStartStopButtonState(TRUE);

            // if we have selected a window and hook a process ID
            if (pOptions->StartWay==COptions::START_WAY_PID)
                // get informations for the last selected window
                hThreadGetRemoteWindowInfos =::CreateThread(NULL,0,GetRemoteWindowInfos,NULL,0,NULL);
        }
    }
    else
    {
        if (!bNoGUI)
            // disable monitoring faking interface
            EnableMonitoringFakingInterface(FALSE);
    }

    if ( (!bNoGUI) && pToolbar )
        // enable StartStop button
        pToolbar->EnableButton(IDC_BUTTON_START_STOP_HOOKS,TRUE);

    SetEvent(evtStartStopUnlocked);
    return bSuccess;
}
BOOL StartStop()
{
    // if application is started, that means we have to do a stop
    return StartStop(!bStarted);
}
//-----------------------------------------------------------------------------
// Name: Stop
// Object: Allow to stop hooking of all hooked processes in a thread.
// Parameters :
//     in  : LPVOID lpParameter : unused
//     out :
//     return : 
//-----------------------------------------------------------------------------
DWORD WINAPI Stop(LPVOID lpParameter)
{
    UNREFERENCED_PARAMETER(lpParameter);

    BOOL bSuccess=TRUE;
    BOOL bItemSuccess;

    CApiOverride* pApiOverride;
    CLinkListItem* pItem;
    CLinkListItem* pNextItem;

    if (IsStopping())
        return 0;

    // set stopping flag
    SetEvent(evtStopping);

    // As soon as user stop, we musn't take command line into account anymore
    bCommandLine=FALSE;
    // free associated command line data
    FreeMemoryAllocatedByCommandLineParsing();

    if (!bNoGUI)
    {
        // lock start/stop button
        pToolbar->EnableButton(IDC_BUTTON_START_STOP_HOOKS,FALSE);
    }


    // if all process spy stop procmon driver first (to avoid hooking new process during stopping query)
    if (pOptions->StartWay==COptions::START_WAY_ALL_PROCESSES)
    {
        // stop ProcMon.sys driver
        pProcMonInterface->StopMonitoring();
        pProcMonInterface->StopDriver();
    }

    
    // for each hooked process
    pApiOverrideList->Lock();
    for(pItem=pApiOverrideList->Head;pItem;pItem=pNextItem)
    {
        pApiOverride=(CApiOverride*)pItem->ItemData;

        pNextItem=pItem->NextItem;  

        // stop
        bItemSuccess=StopApiOverride(pApiOverride);
        if (bItemSuccess)
            DestroyApiOverride(pApiOverride,TRUE);

        bSuccess=bSuccess&&bItemSuccess;
 
    }
    pApiOverrideList->Unlock();

    if (bSuccess)
    {
        if (!bNoGUI)
        {
            SetStartStopButtonState(FALSE);

            // disable monitoring faking interface
            EnableMonitoringFakingInterface(FALSE);
            
            // save monitoring and faking list content
            SaveMonitoringListContent();
            SaveOverridingListContent();

            // Clear monitoring and faking list
            pListViewMonitoringFiles->Clear();
            pListViewOverridingFiles->Clear();

            // clear last selected window informations
            SetWindowText(mhWndDialog,_T("WinAPIOverride32"));

        }

        bMonitoring=TRUE;
        bFaking=TRUE;
        // set the started flag to false
        bStarted=FALSE;
    }

    if (!bNoGUI)
    {
        // unlock start/stop button
        pToolbar->EnableButton(IDC_BUTTON_START_STOP_HOOKS,TRUE);
    }

    ResetEvent(evtStopping);

    if (!bCommandLine)
    {
        // if we have to exit
        if (bExitingStop)
            Exit(NULL);
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Name: SaveMonitoringListContent
// Object: save content for refresh
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void SaveMonitoringListContent()
{
    TCHAR pszPath[MAX_PATH];
    TCHAR psz[MAX_PATH];
    DWORD dwCnt;
    DWORD dwCount;

    dwCount=pListViewMonitoringFiles->GetItemCount();

    // if no files
    if (!dwCount)
        return;

    // clear MonitoringFilesList content
    pOptions->MonitoringFilesList->RemoveAllItems();

    // fill MonitoringFilesList with new files
    for (dwCnt=0;dwCnt<dwCount;dwCnt++)
    {
        pListViewMonitoringFiles->GetItemText(dwCnt,1,pszPath,MAX_PATH);
        pListViewMonitoringFiles->GetItemText(dwCnt,0,psz,MAX_PATH);
        _tcscat(pszPath,_T("\\"));
        _tcscat(pszPath,psz);
        pOptions->MonitoringFilesList->AddItem(pszPath);
    }
}
//-----------------------------------------------------------------------------
// Name: SaveOverridingListContent
// Object: save content for refresh
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void SaveOverridingListContent()
{
    TCHAR pszPath[MAX_PATH];
    TCHAR psz[MAX_PATH];
    DWORD dwCnt;
    DWORD dwCount;

    dwCount=pListViewOverridingFiles->GetItemCount();

    // if no files in list
    if (!dwCount)
        return;

    // clear OverridingDllList content
    pOptions->OverridingDllList->RemoveAllItems();

    // fill OverridingDllList with new files
    for (dwCnt=0;dwCnt<dwCount;dwCnt++)
    {
        pListViewOverridingFiles->GetItemText(dwCnt,1,pszPath,MAX_PATH);
        pListViewOverridingFiles->GetItemText(dwCnt,0,psz,MAX_PATH);
        _tcscat(pszPath,_T("\\"));
        _tcscat(pszPath,psz);
        pOptions->OverridingDllList->AddItem(pszPath);
    }
}

//-----------------------------------------------------------------------------
// Name: CallBackProcessCreation
// Object: call back called at each process creation
// Parameters :
//     in  : HANDLE hParentId : ID of process that create new process 
//           HANDLE hProcessId : new created process ID
//     out :
//     return : 
//-----------------------------------------------------------------------------
void CallBackProcessCreation(HANDLE hParentId,HANDLE hProcessId)
{
    TCHAR psz[MAX_PATH];
    TCHAR psz2[MAX_PATH];
    TCHAR szProcessName[MAX_PATH];
    HANDLE hSnapshot;

    // if user have ask to exit do nothing
    if (bExit)
        return;

    CProcessHelper::SuspendProcess((DWORD)hProcessId);

    // check parent id filters
    if(!pFilters->DoesParentIDMatchFilters((DWORD)hParentId))
    {
        CProcessHelper::ResumeProcess((DWORD)hProcessId);
        return;
    }

    // get process name and check proc name filters
    CProcessName::GetProcessName((DWORD)hProcessId,szProcessName);

    // check filters based on process name
    if (!pFilters->DoesProcessNameMatchFilters(szProcessName))
    {
        CProcessHelper::ResumeProcess((DWORD)hProcessId);
        return;
    }

    if (!bNoGUI)
        // update options from GUI
        GetOptionsFromGUI();

    CApiOverride* pApiOverride;
    // create a new CApiOverride object
    pApiOverride=new CApiOverride(mhWndDialog);

    // set options
    SetBeforeStartOptions(pApiOverride);
    // wait first module loading (= end of nt loader job) before call to InitializeStart and injection
    DWORD OriginalTickCount=GetTickCount();
    DWORD CurrentTickCount;
    CProcessHelper::ResumeProcess((DWORD)hProcessId);

    while(!CProcessHelper::IsFirstModuleLoaded((DWORD)hProcessId))
    {
        Sleep(1);
        CurrentTickCount=GetTickCount();
        if (OriginalTickCount+MAX_POOLING_TIME_IN_MS<CurrentTickCount)
        {
            // try to get process name
            if (CProcessName::GetProcessName((DWORD)hParentId,psz2))
                _sntprintf(psz,MAX_PATH,_T("Error hooking process 0x%p (%s) launched by 0x%p (%s)"),
                hProcessId,szProcessName,hParentId,psz2);
            else
                _sntprintf(psz,MAX_PATH,_T("Error hooking process 0x%p (%s) launched by 0x%p"),
                hProcessId,szProcessName,hParentId);

            ReportMessage(pApiOverride,psz,MSG_ERROR);

            return;
        }
    }

    // if sleeptime before injection
    if ((pOptions->StartAllProcessesbOnlyAfter) && (pOptions->StartAllProcessesOnlyAfterMs!=0))
    {
        // sleep
        // Notice : we don't have to care of this blocking state 
        // as CallBackProcessCreation func are called in threads (see ProcmonInterface)
        Sleep(pOptions->StartAllProcessesOnlyAfterMs);
    }

    // create a snapshot before starting apioverride has it create some threads that belongs to the 
    // target process, so if we don't make a snapshot now,CProcessHelper::SuspendProcess we will 
    // suspend our threads too
    hSnapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);

    // start overriding on new process
    if (!pApiOverride->Start((DWORD)hProcessId))
    {

        // try to get process name
        if (CProcessName::GetProcessName((DWORD)hParentId,psz2))
            _sntprintf(psz,MAX_PATH,_T("Error hooking process 0x%p (%s) launched by 0x%p (%s)"),
                        hProcessId,szProcessName,hParentId,psz2);
        else
            _sntprintf(psz,MAX_PATH,_T("Error hooking process 0x%p (%s) launched by 0x%p"),
                        hProcessId,szProcessName,hParentId);

        // report error
        ReportMessage(pApiOverride,psz,MSG_ERROR);

        // free memory
        DestroyApiOverride(pApiOverride);

        return;
    }

    CProcessHelper::SuspendProcess(hSnapshot,(DWORD)hProcessId);

    SetAfterStartOptions(pApiOverride);

    // report information
    if (CProcessName::GetProcessName((DWORD)hParentId,psz2))
        _sntprintf(psz,MAX_PATH,_T("Process 0x%p (%s) launched by 0x%p (%s) successfully hooked"),
                                hProcessId,szProcessName,hParentId,psz2);
    else
        _sntprintf(psz,MAX_PATH,_T("Process 0x%p (%s) launched by 0x%p successfully hooked"),
                                hProcessId,szProcessName,hParentId);

    ReportMessage(pApiOverride,psz,MSG_INFORMATION);

    pPluginManager->OnProcessHooked(pApiOverride);

    if (!bNoGUI) // notice in case of bNoGui, SetAfterStartOptions assume injection of files
    {
        LoadCurrentMonitoringFiles(pApiOverride);
        LoadCurrentOverridingFiles(pApiOverride);
    }

    CProcessHelper::ResumeProcess(hSnapshot,(DWORD)hProcessId);
    CloseHandle(hSnapshot);
}
BOOL LoadCurrentMonitoringFiles(CApiOverride* pApiOverride)
{
    BOOL bSuccess;
    DWORD dwCnt;
    DWORD dwNbItems;
    TCHAR psz[MAX_PATH];
    TCHAR psz2[MAX_PATH];
    //////////////////////////////////////////
    // inject all monitoring files present in listbox
    //////////////////////////////////////////

    bSuccess = TRUE;
    dwNbItems=pListViewMonitoringFiles->GetItemCount();
    // for each listbox item
    for (dwCnt=0;dwCnt<dwNbItems;dwCnt++)
    {
        // retrieve text
        pListViewMonitoringFiles->GetItemText(dwCnt,0,psz2,MAX_PATH);
        pListViewMonitoringFiles->GetItemText(dwCnt,1,psz,MAX_PATH);

        if (*psz!=0)
        {
            _tcscat(psz,_T("\\"));
            _tcscat(psz,psz2);
        }
        else
            _tcscpy(psz,psz2);

        // load monitoring file
        bSuccess = bSuccess && pApiOverride->LoadMonitoringFile(psz);

    }
    return bSuccess;
}
BOOL LoadCurrentOverridingFiles(CApiOverride* pApiOverride)
{
    BOOL bSuccess;
    DWORD dwCnt;
    DWORD dwNbItems;
    TCHAR psz[MAX_PATH];
    TCHAR psz2[MAX_PATH];
    //////////////////////////////////////////
    // inject all dll present in listbox
    //////////////////////////////////////////
    bSuccess = TRUE;
    dwNbItems=pListViewOverridingFiles->GetItemCount();
    // for each listbox item
    for (dwCnt=0;dwCnt<dwNbItems;dwCnt++)
    {
        // retrieve text
        pListViewOverridingFiles->GetItemText(dwCnt,0,psz2,MAX_PATH);
        pListViewOverridingFiles->GetItemText(dwCnt,1,psz,MAX_PATH);

        if (*psz!=0)
        {
            _tcscat(psz,_T("\\"));
            _tcscat(psz,psz2);
        }
        else
            _tcscpy(psz,psz2);

        // load faking dll
        bSuccess = bSuccess && pApiOverride->LoadFakeAPI(psz);
    }
    return bSuccess;
}
//-----------------------------------------------------------------------------
// Name: GetRemoteWindowInfos
// Object: this proc contains samples of remote process proc execution
//         gives info only for last selected window
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
DWORD WINAPI GetRemoteWindowInfos(PVOID lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    CApiOverride* pApiOverride;

    if (IsStopping())
        return -1;
    if (IsExiting())
        return -1;
        
    pApiOverrideList->Lock();
    
    // if (IsBadReadPtr(pApiOverrideList,sizeof(CLinkList*)))
        // return (DWORD)-1;
    // if (IsBadReadPtr(pApiOverrideList->Head,sizeof(CLinkListItem*)))
        // return (DWORD)-1;
    pApiOverride=(CApiOverride*)pApiOverrideList->Head->ItemData;
    // if (IsBadReadPtr(pApiOverride,sizeof(CApiOverride*)))
        // return (DWORD)-1;

    // give info of selected window
    if (pSelectWindow->WindowHandle)
    {
        TCHAR psz[MAX_PATH];
        TCHAR psztmp[MAX_PATH];

        PBYTE DlgProc=0;
        PBYTE WndProc=0;

        int iParam;
        STRUCT_FUNC_PARAM pParams[2];

        // For the GetWindowLong func with DWLP_DLGPROC or GWLP_WNDPROC param,
        // just check the difference between call from this process and call in remote process

        // DlgProc=(DWORD)GetWindowLong (pSelectWindow->WindowHandle, DWLP_DLGPROC);
        iParam=DWLP_DLGPROC;
        pParams[0].bPassAsRef=FALSE;
        pParams[0].dwDataSize=sizeof(HWND);
        pParams[0].pData=(PBYTE)&pSelectWindow->WindowHandle;
        pParams[1].bPassAsRef=FALSE;
        pParams[1].dwDataSize=sizeof(int);
        pParams[1].pData=(PBYTE)&iParam;
#if (defined(UNICODE)||defined(_UNICODE))
        pApiOverride->ProcessInternalCall(_T("User32.dll"),_T("GetWindowLongW"),2,pParams,&DlgProc,15000);
#else
        pApiOverride->ProcessInternalCall(_T("User32.dll"),_T("GetWindowLongA"),2,pParams,&DlgProc,15000);
#endif
        

        // WndProc=(DWORD)GetWindowLong (pSelectWindow->WindowHandle, GWLP_WNDPROC);
        iParam=GWLP_WNDPROC;
        pParams[0].bPassAsRef=FALSE;
        pParams[0].dwDataSize=sizeof(HWND);
        pParams[0].pData=(PBYTE)&pSelectWindow->WindowHandle;
        pParams[1].bPassAsRef=FALSE;
        pParams[1].dwDataSize=sizeof(int);
        pParams[1].pData=(PBYTE)&iParam;
#if (defined(UNICODE)||defined(_UNICODE))
        pApiOverride->ProcessInternalCall(_T("User32.dll"),_T("GetWindowLongW"),2,pParams,&DlgProc,15000);
#else
        pApiOverride->ProcessInternalCall(_T("User32.dll"),_T("GetWindowLongA"),2,pParams,&DlgProc,15000);
#endif


/*
//////////////////////
// other samples of remote call
//////////////////////
PBYTE Ret;
// api stdcall
pApiOverride->ProcessInternalCall("Kernel32.dll","IsDebuggerPresent",0,NULL,&Ret,15000);
// exe internal cdecl
int i1=18;
int i2=22;
TCHAR pc[255];
pParams[0].bPassAsRef=FALSE;
pParams[0].dwDataSize=4;
pParams[0].pData=(PBYTE)&i1;
pParams[1].bPassAsRef=FALSE;
pParams[1].dwDataSize=4;
pParams[1].pData=(PBYTE)&i2;
pApiOverride->ProcessInternalCall(_T("EXE_INTERNAL@0x4114dd"),_T("MyFunc"),2,pParams,&Ret,15000);
// api stdcall with out pointer
i1=255;
pParams[0].bPassAsRef=FALSE;
pParams[0].dwDataSize=4;
pParams[0].pData=(PBYTE)&i1;
pParams[1].bPassAsRef=TRUE;
pParams[1].dwDataSize=255;
pParams[1].pData=(PBYTE)pc;
pApiOverride->ProcessInternalCall(_T("Kernel32.dll"),_T("GetTempPathA"),2,pParams,&Ret,15000);
// api stdcall with ref struct

RECT Rect;
pParams[0].bPassAsRef=FALSE;
pParams[0].dwDataSize=4;
pParams[0].pData=(PBYTE)&pSelectWindow->WindowHandle;
pParams[1].bPassAsRef=TRUE;
pParams[1].dwDataSize=sizeof(RECT);
pParams[1].pData=(PBYTE)&Rect;
pApiOverride->ProcessInternalCall(_T("User32.dll"),_T("GetWindowRect"),2,pParams,&Ret,15000);
GetWindowRect(pSelectWindow->WindowHandle,&Rect);
*/

        // give window information to user ( WindowProc and WindowDlgProc)
        *psz=0;
        _tcscpy(psz,_T("WinAPIOverride32                 "));
        _stprintf(psztmp,_T("Pid: 0x") _T("%X") _T("   "),pApiOverride->GetProcessID());
        _tcscat(psz,psztmp);
        
        if ((WndProc)||(DlgProc))
        {
            if (DlgProc)
            {
                _stprintf(psztmp,_T("DlgProc: 0x%p   "),DlgProc);
                _tcscat(psz,psztmp);
            }
            if (WndProc)
            {
                _stprintf(psztmp,_T("WndProc: 0x%p   "),WndProc);
                _tcscat(psz,psztmp);
            }
        }
        SetWindowText(mhWndDialog,psz);
    }
    
    pApiOverrideList->Unlock();
    
    CleanThreadHandleIfNotExiting(&hThreadGetRemoteWindowInfos);
    return 0;
}

//-----------------------------------------------------------------------------
// Name: StartStopMonitoring
// Object: start or stop monitoring of all hooked processes
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL StartStopMonitoring(BOOL Start)
{
    BOOL bSuccess=TRUE;
    BOOL bItemSuccess;
    CLinkListItem* pItem;


    if (Start)
    {
        // start monitoring for all hooked processes
        pApiOverrideList->Lock(TRUE);
        for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
        {
            bItemSuccess=((CApiOverride*)pItem->ItemData)->StartMonitoring();
            bSuccess=bSuccess&&bItemSuccess;
        }
        pApiOverrideList->Unlock();
        if (bSuccess)
        {
            bMonitoring=TRUE;
            // put IDC_BUTTON_START_STOP_MONITORING caption to Stop
            pToolbar->ReplaceToolTipText(IDC_BUTTON_START_STOP_MONITORING,_T("Pause Monitoring"));
            pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_MONITORING,CToolbar::ImageListTypeEnable,IDI_ICON_PAUSE_MONITORING);
            pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_MONITORING,CToolbar::ImageListTypeHot,IDI_ICON_PAUSE_MONITORING_HOT);
        }

    }
    else
    {
        // stop monitoring for all hooked processes
        pApiOverrideList->Lock(TRUE);
        for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
        {
            bItemSuccess=((CApiOverride*)pItem->ItemData)->StopMonitoring();
            bSuccess=bSuccess&&bItemSuccess;
        }
        pApiOverrideList->Unlock();
        if (bSuccess)
        {
            bMonitoring=FALSE;
            // put IDC_BUTTON_START_STOP_MONITORING caption to Start
            pToolbar->ReplaceToolTipText(IDC_BUTTON_START_STOP_MONITORING,_T("Resume Monitoring"));
            pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_MONITORING,CToolbar::ImageListTypeEnable,IDI_ICON_RESUME_MONITORING);
            pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_MONITORING,CToolbar::ImageListTypeHot,IDI_ICON_RESUME_MONITORING);

        }
    }

    return bSuccess;
}

void StartStopMonitoring()
{
    StartStopMonitoring(!bMonitoring);
}

//-----------------------------------------------------------------------------
// Name: StartStopFaking
// Object: start or stop faking for all hooked processes
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL StartStopFaking(BOOL Start)
{
    BOOL bSuccess=TRUE;
    BOOL bItemSuccess;
    CLinkListItem* pItem;
   
    if (Start)
    {
        // start faking for all hooked processes
        pApiOverrideList->Lock(TRUE);
        for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
        {
            bItemSuccess=((CApiOverride*)pItem->ItemData)->StartFaking();
            bSuccess=bSuccess&&bItemSuccess;
        }
        pApiOverrideList->Unlock();
        if (bSuccess)
        {
            bFaking=TRUE;
            // put IDC_BUTTON_START_STOP_FAKING caption to Stop
            pToolbar->ReplaceToolTipText(IDC_BUTTON_START_STOP_FAKING,_T("Pause Overriding"));
            pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_FAKING,CToolbar::ImageListTypeEnable,IDI_ICON_PAUSE_FAKING);
            pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_FAKING,CToolbar::ImageListTypeHot,IDI_ICON_PAUSE_FAKING_HOT);
        }
    }
    else
    {
        // stop faking for all hooked processes
        pApiOverrideList->Lock(TRUE);
        for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
        {
            bItemSuccess=((CApiOverride*)pItem->ItemData)->StopFaking();
            bSuccess=bSuccess&&bItemSuccess;
        }
        pApiOverrideList->Unlock();
        if (bSuccess)
        {
            bFaking=FALSE;
            // put IDC_BUTTON_START_STOP_FAKING caption to Stop
            pToolbar->ReplaceToolTipText(IDC_BUTTON_START_STOP_FAKING,_T("Resume Overriding"));
            pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_FAKING,CToolbar::ImageListTypeEnable,IDI_ICON_RESUME_FAKING);
            pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_FAKING,CToolbar::ImageListTypeHot,IDI_ICON_RESUME_FAKING_HOT);
        }
    }

    return bSuccess;
}
void StartStopFaking()
{
    StartStopFaking(!bFaking);
}

//-----------------------------------------------------------------------------
// Name: LoadMonitoringFile
// Object: load a monitoring file in all hooked processes
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void LoadMonitoringFile()
{
    if (IsExiting())
        return;
    if (IsStopping())
        return;
    TCHAR pszFile[MAX_PATH]=_T("");
    GetDlgItemText(mhWndDialog,IDC_EDIT_MONITORING_FILE,pszFile,MAX_PATH);
    LoadMonitoringFile(pszFile);
}

//-----------------------------------------------------------------------------
// Name: LoadMonitoringFile
// Object: load a monitoring file in all hooked processes
// Parameters :
//     in  : TCHAR* pszFile : monitoring file name
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL LoadMonitoringFile(TCHAR* pszFile)
{
    BOOL bSuccess=TRUE;
    BOOL bItemSuccess;
    TCHAR* pszName;
    TCHAR* ppc[2];
    CLinkListItem* pItem;

    // for each hooked process
    pApiOverrideList->Lock(TRUE);
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        // try to load file
        bItemSuccess=((CApiOverride*)pItem->ItemData)->LoadMonitoringFile(pszFile);
        bSuccess=bSuccess&&bItemSuccess;
    }
    pApiOverrideList->Unlock();
    // on success or if all processes injection 
    // (in case of all processes injection, we have to add item to list even if 
    //  an application has failed to load file, because new started processes will use
    //  the list content to inject monitoring files)
    if (bSuccess
        || (GetStartWay()==COptions::START_WAY_ALL_PROCESSES)
        // in case of plugin hooking we can't use START_WAY_ALL_PROCESSES as only one process checking
        || (pApiOverrideList->GetItemsCount()>1)
        )
    {
        if (!bNoGUI)
        {
            // add loaded file full path to list view
            pszName=_tcsrchr(pszFile,'\\');
            if (pszName)
            {
                TCHAR pszPath[MAX_PATH];
                DWORD Size=(DWORD)(pszName-pszFile);
                _tcsncpy(pszPath,pszFile,Size);
                pszPath[Size]=0;
                pszName++;
                ppc[0]=pszName;
                ppc[1]=pszPath;
                pListViewMonitoringFiles->AddItemAndSubItems(2,ppc);
            }
            else
            {
                ppc[0]=pszFile;
                pListViewMonitoringFiles->AddItemAndSubItems(1,ppc);
            }
        }
    }

    return bSuccess;
}

//-----------------------------------------------------------------------------
// Name: UnloadMonitoringFile (called from GUI in a reply to OnUnloadMonitoringFileButtonClick)
// Object: unload a monitoring file in all hooked processes
//          due to unload required time (sometime blocking call)
//          unload is done into a thread
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void UnloadMonitoringFile()
{
    if (hThreadUnloadMonitoringFile)
        return;
    if (IsStopping())
        return;

    // create a new thread to allow unload of monitoring/overriding of DlgProc and SendMessage
    hThreadUnloadMonitoringFile=::CreateThread(NULL,0,UnloadMonitoringFileThread,0,0,NULL);
}

// get listview index for monitoring and faking list from a file full path
//  return index on success, -1 if not found
int GetLoadedFileIndex(CListview* pListView,TCHAR* FileFullPath)
{
    TCHAR FullPath[MAX_PATH];
    SIZE_T Size;
    LONG lNbItems=pListView->GetItemCount();

    // parse listbox items
    for (int cnt=0;cnt<lNbItems;cnt++)
    {
        pListView->GetItemText(cnt,0,FullPath,MAX_PATH);
        _tcscat(FullPath,_T("\\"));
        Size = _tcslen(FullPath);
        pListView->GetItemText(cnt,0,&FullPath[Size],MAX_PATH-Size);

        if (_tcsicmp(FileFullPath,FullPath)==0)
        {
            return cnt;
        }
    }
    return -1;
}
BOOL UnloadMonitoringFile(TCHAR* FileName)
{
    CLinkListItem* pItem;
    BOOL bItemSuccess;
    BOOL bSuccess = TRUE;
    // for each hooked process
    pApiOverrideList->Lock(TRUE);
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        // unload the monitoring file
        bItemSuccess=((CApiOverride*)pItem->ItemData)->UnloadMonitoringFile(FileName);
        bSuccess=bSuccess&&bItemSuccess;
    }
    pApiOverrideList->Unlock();
    return bSuccess;
}
// return TRUE if item should be removed
BOOL UnloadMonitoringFileAndRemoveFromGui(TCHAR* FileName,int ListViewItemIndex)
{
    BOOL bSuccess = UnloadMonitoringFile(FileName);

    // on success or if all processes injection 
    // (in case of all processes injection, we have to add item to list even if 
    //  an application has failed to load file, because new started processes will use
    //  the list content to inject files)
    if (bSuccess 
        || (GetStartWay()==COptions::START_WAY_ALL_PROCESSES) 
        // in case of plugin hooking we can't use START_WAY_ALL_PROCESSES as only one process checking
        || (pApiOverrideList->GetItemsCount()>1)
        )
    {
        // remove item from list view
        pListViewMonitoringFiles->RemoveItem(ListViewItemIndex);
    }
    return bSuccess;
}

// export for plugin
BOOL UnloadMonitoringFileAndRemoveFromGui(TCHAR* FileName)
{
    BOOL bSuccess;
    // lock unload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_UNLOAD_MONITORING_FILE),FALSE);

    // get number of loaded files
    int ListViewItemIndex;
    ListViewItemIndex = GetLoadedFileIndex(pListViewMonitoringFiles,FileName);
    if (ListViewItemIndex>=0)
        bSuccess = UnloadMonitoringFileAndRemoveFromGui(FileName,ListViewItemIndex);
    else
        bSuccess = UnloadMonitoringFile(FileName);

    // unlock unload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_UNLOAD_MONITORING_FILE),TRUE);

    return bSuccess;
}


// lpParameter : 1 all files must be unloaded, 0 only selected files must be unloaded
DWORD WINAPI UnloadMonitoringFileThread(LPVOID lpParameter)
{
    TCHAR pszFile[MAX_PATH];
    TCHAR pszPath[MAX_PATH];
    LONG lNbItems;
    BOOL bItemShouldBeUnhook;

    // get number of loaded files
    lNbItems=pListViewMonitoringFiles->GetItemCount();

    // lock unload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_UNLOAD_MONITORING_FILE),FALSE);

    // parse listbox items
    for (int cnt=lNbItems-1;cnt>=0;cnt--)
    {
        bItemShouldBeUnhook=FALSE;

        // if all file should be unloaded
        if (lpParameter)
            bItemShouldBeUnhook=TRUE;
        else // only selected files must be unloaded
        {
            // if item is selected
            if (pListViewMonitoringFiles->IsItemSelected(cnt))
                bItemShouldBeUnhook=TRUE;
        }

        if (!bItemShouldBeUnhook)
            continue;

        // get monitoring file full path from the listview
        pListViewMonitoringFiles->GetItemText(cnt,0,pszFile,MAX_PATH);
        pListViewMonitoringFiles->GetItemText(cnt,1,pszPath,MAX_PATH);

        _tcscat(pszPath,_T("\\"));
        _tcscat(pszPath,pszFile);

        // try to unload
        UnloadMonitoringFileAndRemoveFromGui(pszPath,cnt);
    }

    // unlock unload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_UNLOAD_MONITORING_FILE),TRUE);

    if (!lpParameter) // if lpParameter == 1 call is not originated from hThreadUnloadMonitoringFile
        CleanThreadHandleIfNotExiting(&hThreadUnloadMonitoringFile);
    return 0;
}

//-----------------------------------------------------------------------------
// Name: LoadOverridingFile
// Object: load a fake dll in all hooked processes
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void LoadOverridingFile()
{
    if (IsStopping())
        return;
    TCHAR pszFile[MAX_PATH]=_T("");
    // get file name to add
    GetDlgItemText(mhWndDialog,IDC_EDIT_FAKING_FILE,pszFile,MAX_PATH);
    LoadOverridingFile(pszFile);
}

//-----------------------------------------------------------------------------
// Name: LoadOverridingFile
// Object: load a fake dll in all hooked processes
// Parameters :
//     in  : TCHAR* pszFile : dll file name
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL LoadOverridingFile(TCHAR* pszFile)
{
    BOOL bSuccess=TRUE;
    BOOL bItemSuccess;
    TCHAR* pszName;
    TCHAR* ppc[2];
    CLinkListItem* pItem;

    // try to load it for all hooked processes
    pApiOverrideList->Lock(TRUE);
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        bItemSuccess=((CApiOverride*)pItem->ItemData)->LoadFakeAPI(pszFile);
        bSuccess=bSuccess&&bItemSuccess;
    }
    pApiOverrideList->Unlock();

    // on success or if all processes injection 
    // (in case of all processes injection, we have to add item to list even if 
    //  an application has failed to load file, because new started processes will use
    //  the list content to inject files)
    if (bSuccess 
        || (GetStartWay()==COptions::START_WAY_ALL_PROCESSES) 
        // in case of plugin hooking we can't use START_WAY_ALL_PROCESSES as only one process checking
        || (pApiOverrideList->GetItemsCount()>1)        
        )
    {
        if (!bNoGUI)
        {
            // add to list
            pszName=_tcsrchr(pszFile,'\\');
            if (pszName)
            {
                TCHAR pszPath[MAX_PATH];
                DWORD Size=(DWORD)(pszName-pszFile);
                _tcsncpy(pszPath,pszFile,Size);
                pszPath[Size]=0;
                pszName++;
                ppc[0]=pszName;
                ppc[1]=pszPath;
                pListViewOverridingFiles->AddItemAndSubItems(2,ppc);
            }
            else
            {
                ppc[0]=pszFile;
                pListViewOverridingFiles->AddItemAndSubItems(1,ppc);
            }
        }
    }
    return bSuccess;
}

//-----------------------------------------------------------------------------
// Name: UnloadOverridingFile (called from GUI in a reply to OnUnloadFakingFileButtonClick)
// Object: unload a monitoring file in all hooked processes
//          due to unload required time (sometime blocking call)
//          unload is done into a thread
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void UnloadOverridingFile()
{
    if (hThreadUnloadOverridingFile)
        return;
    if (IsStopping())
        return;
    // create a new thread to allow unload of monitoring/overriding of DlgProc
    hThreadUnloadOverridingFile=::CreateThread(NULL,0,UnloadOverridingFileThread,NULL,0,NULL);
}

BOOL UnloadOverridingFile(TCHAR* FileName)
{
    CLinkListItem* pItem;
    BOOL bItemSuccess;
    BOOL bSuccess = TRUE;
    // for each hooked process
    pApiOverrideList->Lock(TRUE);
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        // unload the monitoring file
        bItemSuccess=((CApiOverride*)pItem->ItemData)->UnloadFakeAPI(FileName);
        bSuccess=bSuccess&&bItemSuccess;
    }
    pApiOverrideList->Unlock();
    return bSuccess;
}

// return TRUE if item should be removed
BOOL UnloadOverridingFileAndRemoveFromGui(TCHAR* FileName,int ListViewItemIndex)
{
    BOOL bSuccess = UnloadOverridingFile(FileName);

    // on success or if all processes injection 
    // (in case of all processes injection, we have to add item to list even if 
    //  an application has failed to load file, because new started processes will use
    //  the list content to inject files)
    if (bSuccess 
        || (GetStartWay()==COptions::START_WAY_ALL_PROCESSES) 
        // in case of plugin hooking we can't use START_WAY_ALL_PROCESSES as only one process checking
        || (pApiOverrideList->GetItemsCount()>1)
        )
    {
        // remove item from list view
        pListViewOverridingFiles->RemoveItem(ListViewItemIndex);
    }
    return bSuccess;
}

// export for plugin
BOOL UnloadOverridingFileAndRemoveFromGui(TCHAR* FileName)
{
    BOOL bSuccess;
    // lock unload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_UNLOAD_FAKING_FILE),FALSE);

    // get number of loaded files
    int ListViewItemIndex;
    ListViewItemIndex = GetLoadedFileIndex(pListViewOverridingFiles,FileName);
    if (ListViewItemIndex>=0)
        bSuccess = UnloadOverridingFileAndRemoveFromGui(FileName,ListViewItemIndex);
    else
        bSuccess = UnloadOverridingFile(FileName);

    // unlock unload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_UNLOAD_FAKING_FILE),TRUE);

    return bSuccess;
}


// lpParameter : 1 all files must be unloaded, 0 only selected files must be unloaded
DWORD WINAPI UnloadOverridingFileThread(LPVOID lpParameter)
{
    TCHAR pszFile[MAX_PATH];
    TCHAR pszPath[MAX_PATH];
    LONG lNbItems;
    BOOL bItemShouldBeUnhook;

    // get number of loaded files
    lNbItems=pListViewOverridingFiles->GetItemCount();

    // lock unload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_UNLOAD_FAKING_FILE),FALSE);

    // parse items to know if they are selected
    for (int cnt=lNbItems-1;cnt>=0;cnt--)
    {
        bItemShouldBeUnhook=FALSE;
        if (lpParameter)
            bItemShouldBeUnhook=TRUE;
        else
        {
            // if item is selected
            if (pListViewOverridingFiles->IsItemSelected(cnt))
                bItemShouldBeUnhook=TRUE;
        }
        if (!bItemShouldBeUnhook)
            continue;

        // get item text (file name)
        pListViewOverridingFiles->GetItemText(cnt,0,pszFile,MAX_PATH);
        pListViewOverridingFiles->GetItemText(cnt,1,pszPath,MAX_PATH);

        _tcscat(pszPath,_T("\\"));
        _tcscat(pszPath,pszFile);

        // try to unload
        UnloadOverridingFileAndRemoveFromGui(pszPath,cnt);
    }

    // unlock unload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_UNLOAD_FAKING_FILE),TRUE);

     if (!lpParameter) // if lpParameter == 1 call is not originated from hThreadUnloadOverridingFile
        CleanThreadHandleIfNotExiting(&hThreadUnloadOverridingFile);
    return 0;
}

//-----------------------------------------------------------------------------
// Name: CallBackBeforeAppResume
// Object: called after apioverride.dll injection when process is hooked at startup
//         and just before process resume
// Parameters :
//     in  : DWORD dwProcessID : process ID that is suspended
//           PVOID pUserParam : associated CApiOverride object pointer
//     out :
//     return : 
//-----------------------------------------------------------------------------
void STDMETHODCALLTYPE CallBackBeforeAppResume(DWORD dwProcessID,PVOID pUserParam)
{
    // if user have ask to exit do nothing
    if (bExit)
        return;

    CApiOverride* pApiOverride=(CApiOverride*)pUserParam;

    SetAfterStartOptions(pApiOverride);

    ReportHookedProcess(pApiOverride,dwProcessID,TRUE);

    pPluginManager->OnProcessHooked(pApiOverride);

    if (!bNoGUI)
        // enable monitoring faking interface
        EnableMonitoringFakingInterface(TRUE);

    bStarted=TRUE;// fill flag now in case application crash during display of message

    if (!bCommandLine)// if bCommandLine user has already given informations
    {
        TCHAR ProcName[MAX_PATH];
        TCHAR psz[MAX_PATH];

        pApiOverride->GetProcessName(ProcName,MAX_PATH);
        if (*ProcName)
            _sntprintf(psz,MAX_PATH,_T("Application %s (0x") _T("%X") _T(") Ready to be Hooked in a paused state\r\nYou can now load Monitoring and Overriding files\r\nPress OK after having loading files to Resume Application"),ProcName,dwProcessID);
        else
            _sntprintf(psz,MAX_PATH,_T("Application with process ID 0x") _T("%X") _T(" Ready to be Hooked in a paused state\r\nYou can now load Monitoring and Overriding files\r\nPress OK after having loading files to Resume Application"),dwProcessID);

        // cool tricks: msgbox don't return until we click OK
        // --> lets application in suspended state until user close messagebox
        MessageBox(NULL,psz,_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
    }
}

//-----------------------------------------------------------------------------
// Name: CallBackUnexpectedUnload
//       Notice : monitoring process has create a thread for use, so we can do wait operations
// Object: called after when a process is closed and we don't have unhook it
// Parameters :
//     in  : DWORD dwProcessID : process ID that has been closed
//           PVOID pUserParam : associated CApiOverride object pointer
//     out :
//     return : 
//-----------------------------------------------------------------------------
void STDMETHODCALLTYPE CallBackUnexpectedUnload(DWORD dwProcessID,PVOID pUserParam)
{
    CApiOverride* pApiOverride;
    TCHAR psz[2*MAX_PATH];
    TCHAR ProcName[MAX_PATH];

    pApiOverride=(CApiOverride*)pUserParam;

    // if user already have asked to exit do nothing
    if (bExit || (!bStarted))
        return;

    // lock list
    pApiOverrideList->Lock();

    BOOL pApiOverrideStillExists=FALSE;
    CLinkListItem* pItem;
    CApiOverride* pExistingApiOverride;
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        pExistingApiOverride=(CApiOverride*)pItem->ItemData;
        if (pExistingApiOverride==pApiOverride)
        {
            pApiOverrideStillExists=TRUE;
            break;
        }
    }
    if (!pApiOverrideStillExists)
    {
        // unlock list
        pApiOverrideList->Unlock();
        return;
    }

    // retrieve process name before deleting pApiOverride object
    *ProcName=0;
    pApiOverride->GetProcessName(ProcName,MAX_PATH);

    if (*ProcName)
        _sntprintf(psz,2*MAX_PATH,_T("Unload of application 0x") _T("%X") _T(" (%s)"),dwProcessID,ProcName);
    else
        _sntprintf(psz,2*MAX_PATH,_T("Unload of application 0x") _T("%X") ,dwProcessID);
    // report unload
    ReportMessage(pApiOverride,psz,MSG_WARNING);

    pPluginManager->OnProcessUnload(pApiOverride);

    // in case of plugin hooking we can't use START_WAY_ALL_PROCESSES as only one process checking
    if ((pOptions->StartWay!=COptions::START_WAY_ALL_PROCESSES)
        && (pApiOverrideList->GetItemsCount()==1)
        )
    // show a message box only one process is hooked --> call stop
    {
        SetEvent(hevtUnexpectedUnload);

        if (!bNoGUI)
        {
            // disable monitoring faking interface
            EnableMonitoringFakingInterface(FALSE);

            // stop
            // in case of No user interface Stop call is done is in the WinMain function
            CloseHandle(CreateThread(NULL,0,Stop,NULL,0,NULL));

            MessageBox(mhWndDialog,psz,_T("Warning"),MB_OK|MB_ICONWARNING|MB_TOPMOST);
        }
    }

    // do it last at it can be blocking (and so messagebox doesn't appears)
    DestroyApiOverride(pApiOverride,TRUE);

    // unlock list
    pApiOverrideList->Unlock();
}

//-----------------------------------------------------------------------------
// Name: Zip
// Object: Zip FileName to ZipFileName with PrettyName
// Parameters :
//     in  : TCHAR* ZipFileName : full path of the resulting zip file
//           TCHAR* FileName : full path of the file to zip
//           TCHAR* PrettyName : Name of the zipped file in the zip archive
//     out :
//     return : TRUE on success
//-----------------------------------------------------------------------------
BOOL Zip(TCHAR* ZipFileName,TCHAR* FileName,TCHAR* PrettyName)
{
    HZIP hZip=CreateZip(ZipFileName, 0,ZIP_FILENAME);
    if (!hZip)
        return FALSE;
    if (ZipAdd(hZip,PrettyName, FileName,0,ZIP_FILENAME)!=ZR_OK)
    {
        CloseZip(hZip);
        return FALSE;
    }
    CloseZip(hZip);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: UnZip
// Object: UnZip FileNameToUnzip from ZipFileName to UnzippedFileName
// Parameters :
//     in  : TCHAR* ZipFileName : full path of the zip file
//           TCHAR* FileNameToUnzip : name of file to unzip
//           TCHAR* UnzippedFileName : name of extracted file
//     out :
//     return : TRUE on success
//-----------------------------------------------------------------------------
BOOL UnZip(TCHAR* ZipFileName,TCHAR* FileNameToUnzip,TCHAR* UnzippedFileName)
{
    ZIPENTRYT ZipEntry;
    int Index;
    HZIP hZip=OpenZip(ZipFileName,0, ZIP_FILENAME);
    if (!hZip)
        return FALSE;

    if (FindZipItem(hZip,FileNameToUnzip,TRUE,&Index,&ZipEntry)!=ZR_OK)
    {
        TCHAR pszMsg[2*MAX_PATH];
        _stprintf(pszMsg,_T("File %s not found in archive"),FileNameToUnzip);
        MessageBox(mhWndDialog,pszMsg,_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        CloseZip(hZip);
        return FALSE;
    }
    if (UnzipItem(hZip,Index, UnzippedFileName,0,ZIP_FILENAME)!=ZR_OK)
    {
        CloseZip(hZip);
        return FALSE;
    }
    CloseZip(hZip);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: LoadThread
// Object: doing loading in a thread allow user to view loading status
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL bLoadingLogs = FALSE;
BOOL LoadLogsWaitEndOfLoading = FALSE;
DWORD WINAPI LoadThread(LPVOID lpParameter)
{
    if (bLoadingLogs)
    {    
        if (!LoadLogsWaitEndOfLoading )
            CleanThreadHandleIfNotExiting(&hThreadLoadLogs);    
        return -1;
    }
    bLoadingLogs = TRUE;
    TCHAR* pszFileName=(TCHAR*)lpParameter;
    // avoid to open a file in started mode
    if (bStarted)
    {
        MessageBox(mhWndDialog,_T("Please stop hooking before loading a log file"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        delete pszFileName;
         bLoadingLogs = FALSE;
        if (!LoadLogsWaitEndOfLoading )
                CleanThreadHandleIfNotExiting(&hThreadLoadLogs);         
        return (DWORD)-1;
    }

    // removes all current items
    ClearLogs(TRUE);

    if (CStdFileOperations::DoesExtensionMatch(pszFileName,_T("zip")))
    {
        TCHAR UncompressedFileName[MAX_PATH];
        TCHAR FileName[MAX_PATH];
        CStdFileOperations::GetTempFileName(pszFileName,UncompressedFileName);
        // remove full path
        _tcscpy(FileName,CStdFileOperations::GetFileName(pszFileName));
        // remove .zip extension
        FileName[_tcslen(FileName)-4]=0;

        ::SetWaitCursor(TRUE);
        if (!UnZip(pszFileName,FileName,UncompressedFileName))
        {
            ::MessageBox(mhWndDialog,_T("Error unzipping file"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
            // assume temp file can be deleted (no read only mode)
            ::SetFileAttributes(UncompressedFileName,FILE_ATTRIBUTE_NORMAL);
            ::DeleteFile(UncompressedFileName);
            ::SetWaitCursor(FALSE);
            delete pszFileName;
             bLoadingLogs = FALSE;
             if (!LoadLogsWaitEndOfLoading )
                CleanThreadHandleIfNotExiting(&hThreadLoadLogs);
            return (DWORD)-1;
        }

        // assume temp file can be deleted (no read only mode)
        ::SetFileAttributes(UncompressedFileName,FILE_ATTRIBUTE_NORMAL);

        if (!COpenSave::Load(UncompressedFileName))
        {
            ::DeleteFile(UncompressedFileName);
            ::SetWaitCursor(FALSE);
            delete pszFileName;
             bLoadingLogs = FALSE;
            if (!LoadLogsWaitEndOfLoading )
                CleanThreadHandleIfNotExiting(&hThreadLoadLogs);
            return (DWORD)-1;
        }
        ::SetWaitCursor(FALSE);
        ::DeleteFile(UncompressedFileName);
    }
    else
    {
        if (!COpenSave::Load(pszFileName))
        {
            delete pszFileName;
             bLoadingLogs = FALSE;
             
            if (!LoadLogsWaitEndOfLoading )
                CleanThreadHandleIfNotExiting(&hThreadLoadLogs);             
            return (DWORD)-1;
        }
    }

    // set wait mouse cursor
    ::SetWaitCursor(TRUE);

    // disable a new loading
    pToolbar->EnableButton(IDC_BUTTON_LOAD_LOGS,FALSE);

    CLinkListItem* pItem;
    PLOG_LIST_ENTRY pLogListEntry;
    CApiOverride ApiOverride;
    ApiOverride.SetMonitoringListview(pListview);

    pLogList->Lock();
    for(pItem=pLogList->Head;pItem;pItem=pItem->NextItem)
    {
        if (bExit)
            break;
        pLogListEntry=(PLOG_LIST_ENTRY)pItem->ItemData;

        // add items of list to list view
        ApiOverride.AddLogEntry(pLogListEntry,TRUE);

        // update dwLogId for another logging session
        if (dwLogId<=pLogListEntry->dwId)
            dwLogId=pLogListEntry->dwId;
    }
    pLogList->Unlock();

    if (!bExit)
    {
        // restore cursor
        ::SetWaitCursor(FALSE);

        // enable loading
        pToolbar->EnableButton(IDC_BUTTON_LOAD_LOGS,TRUE);
    }
    
    if (!LoadLogsWaitEndOfLoading )
        CleanThreadHandleIfNotExiting(&hThreadLoadLogs);
    delete pszFileName;
    bLoadingLogs = FALSE;
    return 0;
}

//-----------------------------------------------------------------------------
// Name: LoadLogs
// Object: load previously saved logs
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL LoadLogs(TCHAR* LogName,BOOL bWaitEndOfLoading)
{
    if (hThreadLoadLogs)
        return FALSE;
    // avoid to open a file in started mode
    if (bStarted)
        return FALSE;

    TCHAR* pszFile=new TCHAR[MAX_PATH];
    *pszFile=0;

    if (LogName)
    {
        _tcsncpy(pszFile,LogName,MAX_PATH);
        pszFile[MAX_PATH-1]=0;
    }
    else
    {
        // open dialog
        OPENFILENAME ofn;
        memset(&ofn,0,sizeof (OPENFILENAME));
        ofn.lStructSize=sizeof (OPENFILENAME);
        ofn.hwndOwner=mhWndDialog;
        ofn.hInstance=mhInstance;
        ofn.lpstrFilter=_T("Compressed XML(.xml.zip), XML(.xml)\0*.xml.zip;*.xml\0");
        ofn.nFilterIndex = 1;
        ofn.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
        ofn.lpstrDefExt=_T("xml");
        ofn.lpstrFile=pszFile;
        ofn.nMaxFile=MAX_PATH;
        
        if (!GetOpenFileName(&ofn))
            return FALSE;
    }
    LoadLogsWaitEndOfLoading = bWaitEndOfLoading;
    hThreadLoadLogs = ::CreateThread(0,0,LoadThread,pszFile,0,0);
    if (!hThreadLoadLogs)
        return FALSE;
    if (bWaitEndOfLoading)
    {
        WaitForSingleObject(hThreadLoadLogs,INFINITE);
        CleanThreadHandleIfNotExiting(&hThreadLoadLogs);
    }

    return TRUE;
}
void LoadLogs()
{
    LoadLogs(NULL,FALSE);
}


BOOL SaveLogsWaitEndOfSaving = FALSE;
BOOL bSavingLogs = FALSE;
DWORD WINAPI SaveLogsThread(LPVOID lpParam)
{
    if (bSavingLogs)
    {
        if(!SaveLogsWaitEndOfSaving)
            CleanThreadHandleIfNotExiting(&hThreadSaveLogs);
        return -1;
    }
    bSavingLogs = TRUE;
    
    TCHAR* lpstrFile =(TCHAR*)lpParam;
    if (!bNoGUI)
        SetWaitCursor(TRUE);

    if (CStdFileOperations::DoesExtensionMatch(lpstrFile,_T("zip")))
    {
        TCHAR UncompressedFileName[MAX_PATH];
        TCHAR FileName[MAX_PATH];
        CStdFileOperations::GetTempFileName(lpstrFile,UncompressedFileName);
        if (!COpenSave::Save(UncompressedFileName))
        {
            if (!bNoGUI)
                SetWaitCursor(FALSE);
            delete lpstrFile;
            bSavingLogs = FALSE;
            if(!SaveLogsWaitEndOfSaving)
                CleanThreadHandleIfNotExiting(&hThreadSaveLogs);            
            return (DWORD)-1;
        }

        // remove full path
        _tcscpy(FileName,CStdFileOperations::GetFileName(lpstrFile));
        // remove .zip extension
        FileName[_tcslen(FileName)-4]=0;

        if (!Zip(lpstrFile,UncompressedFileName,FileName))
        {
            ::DeleteFile(UncompressedFileName);
            if (!bNoGUI)
            {
                ::MessageBox(mhWndDialog,_T("Error zipping file"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
                SetWaitCursor(FALSE);
            }
            delete lpstrFile;
            bSavingLogs = FALSE;
            if(!SaveLogsWaitEndOfSaving)
                CleanThreadHandleIfNotExiting(&hThreadSaveLogs);            
            return (DWORD)-1;
        }
        ::DeleteFile(UncompressedFileName);
        if (!bNoGUI)
            ::MessageBox(NULL,_T("Save Successfully Completed"),_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
    }
    else
    {
        if (COpenSave::Save(lpstrFile))
        {
            if (!bNoGUI)
                ::MessageBox(NULL,_T("Save Successfully Completed"),_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
        }
    }
    if (!bNoGUI)
        SetWaitCursor(FALSE);

    bSavingLogs = FALSE;
    delete lpstrFile;
    if(!SaveLogsWaitEndOfSaving)
        CleanThreadHandleIfNotExiting(&hThreadSaveLogs);    
    return 0;
}

//-----------------------------------------------------------------------------
// Name: SaveLogs
// Object: save current logs
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL SaveLogs(TCHAR* LogName,BOOL bWaitEndOfSaving)
{
    if (hThreadSaveLogs)
        return FALSE;
    TCHAR* pszFile=new TCHAR[MAX_PATH];
    *pszFile=0;

    if (LogName)
    {
        _tcsncpy(pszFile,LogName,MAX_PATH);
        pszFile[MAX_PATH-1]=0;
    }
    else
    {
        // open dialog
        OPENFILENAME ofn;
        memset(&ofn,0,sizeof (OPENFILENAME));
        ofn.lStructSize=sizeof (OPENFILENAME);
        ofn.hwndOwner=mhWndDialog;
        ofn.hInstance=mhInstance;
        ofn.lpstrFilter=_T("Compressed XML(.xml.zip)\0*.xml.zip\0XML(.xml)\0*.xml\0");
        ofn.nFilterIndex = 1;
        ofn.Flags=OFN_EXPLORER|OFN_NOREADONLYRETURN|OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt=_T("xml");
        ofn.lpstrFile=pszFile;
        ofn.nMaxFile=MAX_PATH;

        if (!GetSaveFileName(&ofn))
            return FALSE;
    }

    SaveLogsWaitEndOfSaving = bWaitEndOfSaving;
    HANDLE hThreadSaveLogs = CreateThread(0,0,SaveLogsThread,pszFile,0,0);
    if (!hThreadSaveLogs)
        return FALSE;
    if (bWaitEndOfSaving)
    {
        WaitForSingleObject(hThreadSaveLogs,INFINITE);
        CleanThreadHandleIfNotExiting(&hThreadSaveLogs);
    }
    
    return TRUE;
}
void SaveLogs()
{
    SaveLogs(NULL,FALSE);
}

//-----------------------------------------------------------------------------
// Name: RemoveSelectedLogs
// Object: Remove log entries selected in the listview
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
DWORD WINAPI RemoveSelectedLogsThread(LPVOID lpParameter);
void RemoveSelectedLogs()
{
    if (hThreadRemoveSelectedLogs)
       return;
    // avoid listview details deadlock
    hThreadRemoveSelectedLogs=::CreateThread(0,0,RemoveSelectedLogsThread,0,0,0);
}
DWORD WINAPI RemoveSelectedLogsThread(LPVOID lpParameter)
{
    if (bRemovingSelectedLogs)
        return -1;
        
    UNREFERENCED_PARAMETER(lpParameter);
    int cnt;
    LOG_LIST_ENTRY* pLogEntry=NULL;

     bRemovingSelectedLogs = TRUE;
        
    HANDLE ph[2];
    DWORD dwRes;

    ph[0]=evtDetailListViewUpdateUnlocked;
    ph[1]=evtApplicationExit;
    // wait for an update event or application exit
    dwRes=::WaitForMultipleObjects(2,ph,FALSE,INFINITE);
    if (dwRes!=WAIT_OBJECT_0)// stop event or error
    {
        bRemovingSelectedLogs = FALSE;
        CleanThreadHandleIfNotExiting(&hThreadRemoveSelectedLogs);
        return -1;    
    }
    
    // set wait mouse cursor
    SetWaitCursor(TRUE);
    // avoid user to change selection during removal
    ::EnableWindow(pListview->GetControlHandle(),FALSE);

    // reset LastSelectedItemIndexInMainView
    LastSelectedItemIndexInMainView = -1;

    // current log is removed --> pLogListviewDetails will point to a bad pointer
    // so empty it
    pLogListviewDetails=NULL;
    pListviewDetails->Clear();

    // for each selected item
    for(cnt=pListview->GetItemCount()-1;cnt>=0;cnt--)
    {
        // if item is not selected
        if (!pListview->IsItemSelected(cnt))
            continue;

        // get item log data
        if(!pListview->GetItemUserData(cnt,(LPVOID*)(&pLogEntry)))
        {
            pListview->SetItemUserData(cnt,0);
            pListview->RemoveItem(cnt);
            continue;
        }

        // remove it from listview before freeing pLogEntry memory by pLogList->RemoveItemFromItemData
        // memory is required for listview repaint
        pListview->SetItemUserData(cnt,0);
        pListview->RemoveItem(cnt);

        if (pLogEntry==0)
            continue;

        if (IsBadReadPtr(pLogEntry,sizeof(LOG_LIST_ENTRY)))
            continue;

        // avoid to access listview during this lock to avoid deadlock with custom draw
        pLogList->Lock();

        if (pLogEntry->Type==ENTRY_LOG)
        {
            if (IsBadReadPtr(pLogEntry->pLog,sizeof(LOG_ENTRY)))
                continue;

            // free log buffer
            CApiOverride::FreeLogEntryStatic(pLogEntry->pLog,mhLogHeap);
        }
        else
        {
            // free user message
            if (pLogEntry->ReportEntry.pUserMsg)
            {
                HeapFree(mhLogHeap,0,pLogEntry->ReportEntry.pUserMsg);
                pLogEntry->ReportEntry.pUserMsg=NULL;
            }
        }

        // remove Item from list
        pLogList->RemoveItemFromItemData(pLogEntry,TRUE);
        pLogList->Unlock();
    }

    // restore cursor
    SetWaitCursor(FALSE);

    bRemovingSelectedLogs = FALSE;
    SetEvent(evtDetailListViewUpdateUnlocked);

    // re enable user action on listview
    ::EnableWindow(pListview->GetControlHandle(),TRUE);

    CleanThreadHandleIfNotExiting(&hThreadRemoveSelectedLogs);
    return 0;
}
DWORD WINAPI RemoveAllLogsEntriesThread(LPVOID lpParam)
{
    BOOL bWaitEndOfClearing = (BOOL)(lpParam);
    RemoveAllLogsEntries();
    if (!bWaitEndOfClearing)
        CleanThreadHandleIfNotExiting(&hThreadRemoveAllLogsEntries);
    return 0;
}
//-----------------------------------------------------------------------------
// Name: RemoveAllLogsEntries
// Object: Remove all logs entries from log list
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void RemoveAllLogsEntries()
{
    if (RemovingAllLogsEntries)
        return;
    RemovingAllLogsEntries=TRUE;

    CLinkListItem* pItem;

    HANDLE ph[2];
    DWORD dwRes;

    ph[0]=evtDetailListViewUpdateUnlocked;
    ph[1]=evtApplicationExit;
    // wait for an update event or application exit
    dwRes=::WaitForMultipleObjects(2,ph,FALSE,INFINITE);
    if (dwRes!=WAIT_OBJECT_0)// stop event or error
        return;

    // set wait mouse cursor
    SetWaitCursor(TRUE);
    // avoid user to change selection during removal
    ::EnableWindow(pListview->GetControlHandle(),FALSE);        
        
    // when pLogList and pApiOverrideList must be locked together,
    // lock pApiOverrideList before pLogList to avoid dead locks
    pApiOverrideList->Lock(FALSE);

    // disable use of monitoring log heap of CApiOverride objects
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        ((CApiOverride*)pItem->ItemData)->WaitAndLockMonitoringLogHeap();
    }

    // wait for the end of loading or saving operation
    pLogList->Lock(FALSE);

    // reset log ID
    dwLogId=0;

    if (!bExit)
    {
        // clear listview BEFORE freeing memory
        if (pListview)
            pListview->Clear();

        // remove analysis checked state
        if (pToolbar)
            pToolbar->SetButtonState(IDC_BUTTON_ANALYSIS,pToolbar->GetButtonState(IDC_BUTTON_ANALYSIS)&(~TBSTATE_CHECKED));

        // allow column sorting again
        if (pListview)
            pListview->EnableColumnSorting(TRUE);
    }

    // high speed way (MSDN : you don't need to destroy allocated memory by calling HeapFree before calling HeapDestroy)
    // create a new heap 
    HANDLE TmpHeap=HeapCreate(0,0,0); 
    // set it for all CApiObjects
    
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        ((CApiOverride*)pItem->ItemData)->SetMonitoringLogHeap(TmpHeap);
    }

    if (pAnalysisInfoMessages) // only if ! bNoGui
        pAnalysisInfoMessages->Lock(FALSE);


    // destroy current log heap
    HeapDestroy(mhLogHeap);

    // "clear" linked list content
    pLogList->ReportHeapDestruction();

    if (pAnalysisInfoMessages) // only if ! bNoGui
        pAnalysisInfoMessages->ReportHeapDestruction();

    // update mhLogHeap
    mhLogHeap=TmpHeap;

    // set new heap for linked lists
    pLogList->SetHeap(mhLogHeap);
    pLogList->Unlock();

    if (pAnalysisInfoMessages) // only if ! bNoGui
    {
        pAnalysisInfoMessages->SetHeap(mhLogHeap);
        pAnalysisInfoMessages->Unlock();
    }    

    // re enable use of monitoring log heap of CApiOverride objects
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        ((CApiOverride*)pItem->ItemData)->UnlockMonitoringLogHeap();
    }

    pApiOverrideList->Unlock();

    // restore cursor
    SetWaitCursor(FALSE);

    RemovingAllLogsEntries=FALSE;
    SetEvent(evtDetailListViewUpdateUnlocked);
    
    
       // re enable user action on listview
    ::EnableWindow(pListview->GetControlHandle(),TRUE);    
}

//-----------------------------------------------------------------------------
// Name: ClearLogs
// Object: clear the list view,remove all entries from pLogList, reset Id counter
// Parameters :
//     in  : BOOL bWaitEndOfClearing : TRUE for blocking operation
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL ClearLogsWaitEndOfClearing= FALSE;
void ClearLogs(BOOL bWaitEndOfClearing)
{
    if (hThreadRemoveAllLogsEntries)
        return;
    hThreadRemoveAllLogsEntries=::CreateThread(NULL,0,RemoveAllLogsEntriesThread,(LPVOID)bWaitEndOfClearing,0,NULL);
    // free associated memory
    if (bWaitEndOfClearing)
    {
        WaitForSingleObject(hThreadRemoveAllLogsEntries,INFINITE);
        CleanThreadHandleIfNotExiting(&hThreadRemoveAllLogsEntries);
    }
}
//-----------------------------------------------------------------------------
// Name: Exit
// Object: Ends application. Stop hooking if not done
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
DWORD WINAPI Exit(LPVOID lpParam)
{
    if (IsExiting())
        return -1;
    INT_PTR nExitCode=(INT_PTR)lpParam;
    BOOL bShouldStop;
    // stop keyboard hook to avoid user interactions
    if (hKeyboardHook)
    {
        UnhookWindowsHookEx(hKeyboardHook);
        hKeyboardHook=NULL;
    }

    if (evtApplicationExit)
        SetEvent(evtApplicationExit);

    // hide window to avoid user interaction
    ShowWindow(mhWndDialog,FALSE);

    bExit=TRUE;
    
    // assume bStarted is meaningfully
    WaitForSingleObject(evtStartStopUnlocked,INFINITE);
    bShouldStop=bStarted && (!bExitingStop);
    SetEvent(evtStartStopUnlocked);

    // if apioverriding is started, we have to stop it
    if (bShouldStop)
    {
        // set the bExitingStop flag to TRUE (it will be used in Stop proc)
        bExitingStop=TRUE;
        // call the Stop proc in another thread as Stop func us DlgProc msg and we already are in DlgProc
        CloseHandle(CreateThread(NULL,0,Stop,NULL,0,NULL));
        // dialog exit will be done in the Stop thread calling this Exit func,
        // it's ok as bStarted will be to FALSE, so just return
        return 0;
    }

    // wait end of listview update / logs removing which require listview handles
    WaitForSingleObject(evtDetailListViewUpdateUnlocked,INFINITE);

    // wait for thread ends avoiding deadlocks
    AssumeThreadEnd(&hThreadGetRemoteWindowInfos,20000);
    AssumeThreadEnd(&hThreadUnloadMonitoringFile,20000);
    AssumeThreadEnd(&hThreadUnloadOverridingFile,20000);
    AssumeThreadEnd(&hThreadLoadLogs,INFINITE);
    AssumeThreadEnd(&hThreadSaveLogs,INFINITE);
    AssumeThreadEnd(&hThreadRemoveAllLogsEntries,INFINITE);
    AssumeThreadEnd(&hThreadRemoveSelectedLogs,INFINITE);
    AssumeThreadEnd(&hThreadCallAnalysis,INFINITE);
    AssumeThreadEnd(&hThreadReloadMonitoring,20000);
    AssumeThreadEnd(&hThreadReloadOverriding,20000);
    
    DragAcceptFiles(mhWndDialog, FALSE);

    // store last user changes to save them next
    GetOptionsFromGUI();

    pPluginManager->UninitializeAndUnload();

    // delete listview object
    if (pListview)
    {
        delete pListview;
        pListview=NULL;
    }
    if (pToolbar)
    {
        delete pToolbar;
        pToolbar=NULL;
    }
    if (pRebar)
    {
        delete pRebar;
        pRebar=NULL;
    }
    if (pSplitterLoadMonitoringAndFaking)
    {
        delete pSplitterLoadMonitoringAndFaking;
        pSplitterLoadMonitoringAndFaking=NULL;
    }
    if (pSplitterDetails)
    {
        delete pSplitterDetails;
        pSplitterDetails=NULL;
    }
    if (pSplitterConfig)
    {
        delete pSplitterConfig;
        pSplitterConfig=NULL;
    }

    if (pListViewOverridingFiles)
    {
        delete pListViewOverridingFiles;
        pListViewOverridingFiles=NULL;
    }

    if (pListViewMonitoringFiles)
    {
        delete pListViewMonitoringFiles;
        pListViewMonitoringFiles=NULL;
    }

    if (pListviewDetails)
    {
        delete pListviewDetails;
        pListviewDetails=NULL;
    }
    if (pListviewDetailsTypesDisplay)
    {
        delete pListviewDetailsTypesDisplay;
        pListviewDetailsTypesDisplay = NULL;
    }
    if (pExportPopUpMenu)
    {
        delete pExportPopUpMenu;
        pExportPopUpMenu=NULL;
    }
    if (pCOMToolsPopUpMenu)
    {
        delete pCOMToolsPopUpMenu;
        pCOMToolsPopUpMenu=NULL;
    }
    if (pPluginsPopUpMenu)
    {
        delete pPluginsPopUpMenu;
        pPluginsPopUpMenu = NULL;
    }
    if (pErrorMessagePopUpMenu)
    {
        delete pErrorMessagePopUpMenu;
        pErrorMessagePopUpMenu=NULL;
    }
    if (pToolTipEditModulesExclusionFilters)
    {
        delete pToolTipEditModulesExclusionFilters;
        pToolTipEditModulesExclusionFilters=NULL;
    }
    if (pToolTipReloadModulesExclusionFilters)
    {
        delete pToolTipReloadModulesExclusionFilters;
        pToolTipReloadModulesExclusionFilters=NULL;
    }
    if (pToolTipMonitoringWizard)
    {
        delete pToolTipMonitoringWizard;
        pToolTipMonitoringWizard=NULL;
    }
    if (pToolTipMonitoringReloadLastSession)
    {
        delete pToolTipMonitoringReloadLastSession;
        pToolTipMonitoringReloadLastSession=NULL;
    }
    if (pToolTipFakingReloadLastSession)
    {
        delete pToolTipFakingReloadLastSession;
        pToolTipFakingReloadLastSession=NULL;
    }
    if (hIconWizard)
    {
        DestroyIcon(hIconWizard);
        hIconWizard=NULL;
    }
    if (hIconRefresh)
    {
        DestroyIcon(hIconRefresh);
        hIconRefresh=NULL;
    }
    if (hIconEdit)
    {
        DestroyIcon(hIconEdit);
        hIconEdit=NULL;
    }

    ShellWatcher.StopWatching();

    // end dialog
    EndDialog(mhWndDialog,nExitCode);

    return 0;
}

//-----------------------------------------------------------------------------
// Name: About
// Object: Show the about dialog
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void About()
{
    ShowAboutDialog(mhInstance,mhWndDialog);
}

//-----------------------------------------------------------------------------
// Name: SetApplicationsFilters
// Object: Show the filter dialog and apply new filters
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void SetApplicationsFilters()
{
    DWORD dwCnt;
    DWORD dwCnt2;
    TCHAR psz[2*MAX_PATH];
    BOOL bItemSuccess;
    DWORD dwProcId;
    CLinkListItem* pItem;
    CLinkListItem* pNextItem;
    
    
    CApiOverride* pApiOverride;

    // if all process watching is started
    if (bStarted && (pOptions->StartWay==COptions::START_WAY_ALL_PROCESSES))
    {
        /////////////////////////////////////
        // update pFilters->pdwFiltersCurrentPorcessID 
        // with new hooked process before showing the filter dialog
        /////////////////////////////////////

        // delete current pFilters->pdwFiltersCurrentPorcessID array
        delete[] pFilters->pdwFiltersCurrentPorcessID;

        // get hooked running processes
        
        pApiOverrideList->Lock(FALSE);

        // allocate memory for pdwFiltersCurrentPorcessID and fill pdwFiltersCurrentPorcessIDSize
        pFilters->pdwFiltersCurrentPorcessIDSize=pApiOverrideList->GetItemsCount();
        pFilters->pdwFiltersCurrentPorcessID=new DWORD[pFilters->pdwFiltersCurrentPorcessIDSize];

        // copy hooked ProcessId value into pFilters->pdwFiltersCurrentPorcessID
        dwCnt=0;
        for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
        {
            pFilters->pdwFiltersCurrentPorcessID[dwCnt]=((CApiOverride*)pItem->ItemData)->GetProcessID();
            dwCnt++;
        }
        pApiOverrideList->Unlock();
    }

    /////////////////////////////////////
    // show the filter dialog
    /////////////////////////////////////
    if (pFilters->ShowFilterDialog(mhInstance,mhWndDialog)!=FILTERS_DLG_RES_OK)
        return;

    // if all process watching is not started
    if ((!bStarted) || (pOptions->StartWay!=COptions::START_WAY_ALL_PROCESSES))
        return;

    /////////////////////////////////////
    // if Started, apply changes immediately by hooking new process
    // and unhooking no more used process
    /////////////////////////////////////


    ///////////////////////////////////
    // Hook new processes
    ///////////////////////////////////

    // if api override is stared, hook new process and release no more wanted one
    for(dwCnt=0;dwCnt<pFilters->pdwFiltersNewPorcessIDToWatchSize;dwCnt++)
    {
        // if process is no more alive
        if (!CProcessHelper::IsAlive(pFilters->pdwFiltersNewPorcessIDToWatch[dwCnt]))
        {
            _sntprintf(psz,MAX_PATH,_T("Process 0x") _T("%X") _T(" don't exists anymore, so it won't be hooked"),pFilters->pdwFiltersCurrentPorcessID[dwCnt]);
            ReportMessage(psz,MSG_WARNING);

            continue;
        }

        // create a new CApiOverride object
        pApiOverride=new CApiOverride(mhWndDialog);

        // set options
        SetBeforeStartOptions(pApiOverride);

        if (pApiOverride->Start(pFilters->pdwFiltersNewPorcessIDToWatch[dwCnt]))
        {
            // add information to listview
            ReportHookedProcess(pApiOverride,pFilters->pdwFiltersNewPorcessIDToWatch[dwCnt],TRUE);

            // load monitoring files and overriding dll to new hooked process
            LoadCurrentMonitoringFiles(pApiOverride);

            LoadCurrentOverridingFiles(pApiOverride);

            // as SetAfterStartOptions can add monitoring files and overriding dll to listview, 
            // call it only after listview parsing
            SetAfterStartOptions(pApiOverride);

            pPluginManager->OnProcessHooked(pApiOverride);
        }
        else
        {
            // add information to listview
            ReportHookedProcess(pApiOverride,pFilters->pdwFiltersNewPorcessIDToWatch[dwCnt],FALSE);

            // delete pApiOverride object
            DestroyApiOverride(pApiOverride);   
        }
    }

    ///////////////////////////////////
    // unhook no more wanted processes
    ///////////////////////////////////

    // loop through pApiOverrideList (hooked processes list)
    pApiOverrideList->Lock();
    for(pItem=pApiOverrideList->Head;pItem;pItem=pNextItem)
    {
        pApiOverride=(CApiOverride*)pItem->ItemData;

        // save next item as item can be removed
        pNextItem=pItem->NextItem;

        // retrieves proc id
        dwProcId=pApiOverride->GetProcessID();

        // search proc to release throw pFilters->pdwFiltersPorcessIDToRelease
        for(dwCnt2=0;dwCnt2<pFilters->pdwFiltersPorcessIDToReleaseSize;dwCnt2++)
        {
            if (dwProcId!=pFilters->pdwFiltersPorcessIDToRelease[dwCnt2])
                continue;

            // else
            // process to unhook as been found

            // stop hook
            bItemSuccess=StopApiOverride(pApiOverride);
            if (bItemSuccess)
                DestroyApiOverride(pApiOverride,TRUE);
            
            // item is found, so go out of 2nd loop
            break;
        }
    }
    pApiOverrideList->Unlock();
}

//-----------------------------------------------------------------------------
// Name: SetModulesFilters
// Object: show the module filter dialog. Modules filters are apply inside
//          modulefilters.cpp throw the CModulesFilters class
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void SetModulesFilters()
{
    CModulesFilters ModulesFilters;
    ModulesFilters.ShowFilterDialog(mhInstance,mhWndDialog);
}

//-----------------------------------------------------------------------------
// Name: LogOnlyBaseModule
// Object: apply the log only base module option depending the IDC_CHECK_LOG_ONLY_BASEMODULE state
//          for all started CApiOverride objects
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
BOOL LogOnlyBaseModule(BOOL OnlyBaseModule)
{
    CLinkListItem* pItem;
    BOOL bSuccess = TRUE;

    // store new state
    pOptions->bOnlyBaseModule=OnlyBaseModule;

    // check item in case of non ui call (like plug in)
    CDialogHelper::SetButtonCheckState(::GetDlgItem(mhWndDialog,IDC_CHECK_LOG_ONLY_BASEMODULE),OnlyBaseModule);

    // loop through pApiOverrideList (hooked processes list)
    pApiOverrideList->Lock(TRUE);
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        if (!((CApiOverride*)pItem->ItemData)->LogOnlyBaseModule(pOptions->bOnlyBaseModule))
            bSuccess = FALSE;
    }
    pApiOverrideList->Unlock();

    // enable or disable more specific filters
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MODULES_FILTERS),!pOptions->bOnlyBaseModule);
    pToolbar->EnableButton(IDC_BUTTON_MODULES_FILTERS,!pOptions->bOnlyBaseModule);
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MODULES_FILTERS),!pOptions->bOnlyBaseModule);
    EnableWindow(GetDlgItem(mhWndDialog,IDC_CHECK_USE_MODULE_LIST),!pOptions->bOnlyBaseModule);
    EnableWindow(GetDlgItem(mhWndDialog,IDC_RADIO_EXCLUSION_LIST),!pOptions->bOnlyBaseModule);
    EnableWindow(GetDlgItem(mhWndDialog,IDC_RADIO_INCLUSION_LIST),!pOptions->bOnlyBaseModule);
    
    return bSuccess;
}
void LogOnlyBaseModule()
{
    BOOL bLogOnly = (IsDlgButtonChecked(mhWndDialog,IDC_CHECK_LOG_ONLY_BASEMODULE)==BST_CHECKED);
    LogOnlyBaseModule(bLogOnly);
}

//-----------------------------------------------------------------------------
// Name: LogUseModuleList
// Object: filters or unfilter list of modules depending IDC_CHECK_USE_MODULE_LIST
//         state for all started CApiOverride objects
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void LogUseModuleList()
{
    CLinkListItem* pItem;
    CApiOverride* pApiOverride;

    pOptions->bUseFilterModulesFileList=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_USE_MODULE_LIST)==BST_CHECKED);
    pOptions->bFilterModulesFileListIsExclusionList=(IsDlgButtonChecked(mhWndDialog,IDC_RADIO_EXCLUSION_LIST)==BST_CHECKED);
    // update module list
    if (pOptions->bFilterModulesFileListIsExclusionList)
        GetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->ExclusionFiltersModulesFileList,MAX_PATH);
    else
        GetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->InclusionFiltersModulesFileList,MAX_PATH);

    pApiOverrideList->Lock(TRUE);
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        pApiOverride=((CApiOverride*)pItem->ItemData);

        // apply the log option
        if (pOptions->bUseFilterModulesFileList)
        {
            // set filtering type
            if (pOptions->bFilterModulesFileListIsExclusionList)
            {
                pApiOverride->SetModuleFilteringWay(FILTERING_WAY_NOT_SPECIFIED_MODULES);
                // load module list
                pApiOverride->AddToFiltersModuleList(pOptions->ExclusionFiltersModulesFileList);
            }
            else
            {
                pApiOverride->SetModuleFilteringWay(FILTERING_WAY_ONLY_SPECIFIED_MODULES);
                // load module list
                pApiOverride->AddToFiltersModuleList(pOptions->InclusionFiltersModulesFileList);
            }
        }
        else
        {
            // set filtering way
            pApiOverride->SetModuleFilteringWay(FILTERING_WAY_DONT_USE_LIST);
        }
    }
    pApiOverrideList->Unlock();
}


//-----------------------------------------------------------------------------
// Name: SetMonitoringFiltersState
// Object: enable or disable monitoring filters for all started CApiOverride objects
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void SetMonitoringFiltersState()
{
    CLinkListItem* pItem;
    pOptions->bFiltersApplyToMonitoring=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_FILTERS_APPLY_TO_MONITORING)==BST_CHECKED);

    // loop through pApiOverrideList (hooked processes list)
    pApiOverrideList->Lock(TRUE);
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        ((CApiOverride*)pItem->ItemData)->SetMonitoringModuleFiltersState(pOptions->bFiltersApplyToMonitoring);
    }
    pApiOverrideList->Unlock();
}

//-----------------------------------------------------------------------------
// Name: SetFakingFiltersState
// Object: enable or disable faking filters for all started CApiOverride objects
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void SetFakingFiltersState()
{
    CLinkListItem* pItem;
    pOptions->bFiltersApplyToOverriding=(IsDlgButtonChecked(mhWndDialog,IDC_CHECK_FILTERS_APPLY_TO_FAKING)==BST_CHECKED);
    
    // loop through pApiOverrideList (hooked processes list)
    pApiOverrideList->Lock(TRUE);
    for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        ((CApiOverride*)pItem->ItemData)->SetFakingModuleFiltersState(pOptions->bFiltersApplyToOverriding);
    }
    pApiOverrideList->Unlock();

}

//-----------------------------------------------------------------------------
// Name: StartStopCOMHooking
// Object: enable or disable COM hooking for all ApiOverride objects
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
BOOL StartStopCOMHooking(BOOL Start)
{

    // check item in case of non ui call (like plug in)
    BYTE State = pToolbar->GetButtonState(IDC_BUTTON_COM);
    if (Start)
        State |= TBSTATE_CHECKED;
    else 
        State &= ~TBSTATE_CHECKED;
    pToolbar->SetButtonState(IDC_BUTTON_COM,State);

    BOOL bSuccess = TRUE;
    pOptions->bComAutoHookingEnabled=Start;
    pCOMToolsPopUpMenu->SetEnabledState(idMenuComToolsHookedComObjectInteraction,
                                        bStarted && pOptions->bComAutoHookingEnabled);
    // loop through pApiOverrideList (hooked processes list)
    CLinkListItem* pItem;
    CApiOverride* pApiOverride;

    for (pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        pApiOverride=(CApiOverride*)pItem->ItemData;

        // if start COM, assume that options are set
        if (pOptions->bComAutoHookingEnabled)
            if(!pApiOverride->SetCOMOptions(&pOptions->ComOptions))
                continue;

        if (!pApiOverride->EnableCOMAutoHooking(pOptions->bComAutoHookingEnabled))
            bSuccess = FALSE;
    }

    return bSuccess;
}
void StartStopCOMHooking()
{
    BOOL Start = (pToolbar->GetButtonState(IDC_BUTTON_COM)&TBSTATE_CHECKED);
    StartStopCOMHooking(Start);
}

//-----------------------------------------------------------------------------
// Name: StartStopNETHooking
// Object: enable or disable NET hooking for all ApiOverride objects
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
BOOL StartStopNETHooking(BOOL Start)
{
    // check item in case of non ui call (like plug in)
    BYTE State = pToolbar->GetButtonState(IDC_BUTTON_NET);
    if (Start)
        State |= TBSTATE_CHECKED;
    else 
        State &= ~TBSTATE_CHECKED;
    pToolbar->SetButtonState(IDC_BUTTON_NET,State);


    pOptions->bNetProfilingEnabled=Start;
    // loop through pApiOverrideList (hooked processes list)
    CLinkListItem* pItem;
    CApiOverride* pApiOverride;
    BOOL bSuccess = TRUE;

    if (!CApiOverride::EnableNETProfilingStatic(pOptions->bNetProfilingEnabled))
    {
        if (pOptions->bNetAutoHookingEnabled)
        {
            // restore button unchecked state
            BYTE bState=pToolbar->GetButtonState(IDC_BUTTON_NET);
            bState&=~TBSTATE_CHECKED;
            pToolbar->SetButtonState(IDC_BUTTON_NET,bState);
            // don't try to set option nor state
            return FALSE;
        }
    }

    pApiOverrideList->Lock();
    for (pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        pApiOverride=(CApiOverride*)pItem->ItemData;

        // if start NET, assume that options are set with last values
        if (pOptions->bNetProfilingEnabled)
            // MUST BE SET BEFORE CALLONG EnableNetAutoHooking
            if(!pApiOverride->SetNetOptions(&pOptions->NetOptions))
                continue;

        if (!pApiOverride->EnableNetAutoHooking(pOptions->bNetAutoHookingEnabled 
                                           && pOptions->bNetProfilingEnabled // if user disable .Net profiling stop auto hooking
                                           ))
            bSuccess = FALSE;
    }
    pApiOverrideList->Unlock();
    
    return bSuccess;
}

void StartStopNETHooking()
{
    BOOL Start = (pToolbar->GetButtonState(IDC_BUTTON_NET)&TBSTATE_CHECKED);
    StartStopNETHooking(Start);
}

//-----------------------------------------------------------------------------
// Name: ShowOptions
// Object: show options dialog
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ShowOptions()
{
    COptionsUI pOptionDialog(pOptions);
    if (pOptionDialog.Show(mhInstance,mhWndDialog)==COptionsUI::DLG_RES_OK)
    {
        // loop through pApiOverrideList (hooked processes list)
        CLinkListItem* pItem;
        pApiOverrideList->Lock(TRUE);
        for(pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
        {
            ApplyGeneralOptions((CApiOverride*)pItem->ItemData,pOptions);
        }
        pApiOverrideList->Unlock();
    }
    
}
//-----------------------------------------------------------------------------
// Name: ApplyCOMOptions
// Object: Apply options of COM option dialog only to pAPIOverride object
// Parameters :
//     in  : CAPIOverride* pAPIOverride : object onto apply options
//     out :
//     return : TRUE in case of success, FALSE on error
//-----------------------------------------------------------------------------
BOOL ApplyCOMOptions(CApiOverride* pApiOverride,COptions* Options)
{
    if(!pApiOverride->SetCOMOptions(&Options->ComOptions))
        return FALSE;

    return pApiOverride->EnableCOMAutoHooking(Options->bComAutoHookingEnabled);
}

//-----------------------------------------------------------------------------
// Name: ApplyNETOptions
// Object: Apply options of NET option dialog only to pAPIOverride object
// Parameters :
//     in  : CAPIOverride* pAPIOverride : object onto apply options
//     out :
//     return : TRUE in case of success, FALSE on error
//-----------------------------------------------------------------------------
BOOL ApplyNETOptions(CApiOverride* pApiOverride,COptions* Options)
{
    // MUST BE SET BEFORE CALLONG EnableNetAutoHooking
    if(!pApiOverride->SetNetOptions(&Options->NetOptions))
        return FALSE;

    return pApiOverride->EnableNetAutoHooking(Options->bNetAutoHookingEnabled);
}


//-----------------------------------------------------------------------------
// Name: ApplyGeneralOptions
// Object: Apply options of option dialog only to pAPIOverride object
// Parameters :
//     in  : CAPIOverride* pAPIOverride : object onto apply options
//     out :
//     return : TRUE in case of success, FALSe on error
//-----------------------------------------------------------------------------
BOOL ApplyGeneralOptions(CApiOverride* pApiOverride,COptions* Options)
{
    if (!pApiOverride->SetAutoAnalysis(Options->AutoAnalysis))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots
    if(!pApiOverride->SetCallStackRetrieval(Options->bLogCallStack,Options->CallStackEbpRetrievalSize))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots
    if(!pApiOverride->SetMonitoringFileDebugMode(Options->bMonitoringFileDebugMode))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots
    if(!pApiOverride->LogOnlyBaseModule(Options->bOnlyBaseModule))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots
    if(!pApiOverride->BreakDialogDontBreakApioverrideThreads(Options->BreakDialogDontBreakAPIOverrideThreads))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots
    if(!pApiOverride->SetCOMOptions(&Options->ComOptions))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots
    if(!pApiOverride->SetNetOptions(&Options->NetOptions))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots
    if(!pApiOverride->EnableNetAutoHooking(Options->bNetAutoHookingEnabled))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots
    if(!pApiOverride->AllowTlsCallbackHooking(Options->bAllowTlsCallbackHooks))
        return FALSE;// return on error to avoid multiple same error source like error writing mailslots

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: SetBeforeStartOptions
// Object: Apply all options of COptions class to pAPIOverride object
// Parameters :
//     in  : CAPIOverride* pAPIOverride : object onto apply options
//     out :
//     return : TRUE in case of success, FALSe on error
//-----------------------------------------------------------------------------
BOOL SetBeforeStartOptions(CApiOverride* pApiOverride,COptions* Options)
{
    // apply current heap to pApiOverride object
    pApiOverride->SetMonitoringLogHeap(mhLogHeap);

    if (!ApplyGeneralOptions(pApiOverride,Options))
        return FALSE;

    // set the unexpected unload callback
    pApiOverride->SetUnexpectedUnloadCallBack(CallBackUnexpectedUnload,pApiOverride);

    // if user interface
    if (!bNoGUI)
    {
        // give handle of listview to pApiOverride so we don't worry about logging monitoring msg
        pApiOverride->SetMonitoringListview(pListview);
    }

    // Set log entry callback
    pApiOverride->SetMonitoringCallback(CallbackMonitoringLog,pApiOverride,TRUE);
    // set the report callback
    pApiOverride->SetReportMessagesCallBack(CallBackReportMessage,pApiOverride);
    // set the 
    pApiOverride->SetOverridingDllQueryCallBack(CallBackOverridingDllQuery,pApiOverride);

    // set COM options
    if (!ApplyCOMOptions(pApiOverride,Options))
        return FALSE;

    // set .NET options
    if (!ApplyNETOptions(pApiOverride,Options))
        return FALSE;

    return TRUE;
}
BOOL SetBeforeStartOptions(CApiOverride* pApiOverride)
{
    return SetBeforeStartOptions(pApiOverride,pOptions);
}

//-----------------------------------------------------------------------------
// Name: SetAfterStartOptions
// Object: Do some operations according to pOptions after pApiOverride was successfully started
// Parameters :
//     in  : CAPIOverride* pAPIOverride : object onto apply options
//     out :
//     return : TRUE in case of success, FALSE on error
//-----------------------------------------------------------------------------
BOOL SetAfterStartOptions(CApiOverride* pApiOverride,COptions* Options)
{
    // if no user interface
    if (bNoGUI)
    {
        // if no monitoring file was specified in command line 
        // --> only dll faking
        // --> disable monitoring to earn some time
        if ((CommandLineMonitoringFilesArraySize==0))
        {
            // try earn some time
            if (!pApiOverride->StopMonitoring()) 
                return FALSE;
        }
    }

    // set monitoring and faking state
    if (!bMonitoring)
    {
        if (!pApiOverride->StopMonitoring())
            return FALSE;
    }
    if (!bFaking)
    {
        if(!pApiOverride->StopFaking())
            return FALSE;
    }

    // set monitoring filter state
    if (!pApiOverride->SetMonitoringModuleFiltersState(Options->bFiltersApplyToMonitoring))
        return FALSE;
    // set faking filter state
    if (!pApiOverride->SetFakingModuleFiltersState(Options->bFiltersApplyToOverriding))
        return FALSE;

    // add it to pApiOverrideList
    pApiOverrideList->AddItem(pApiOverride);

    // add not logged module list if defined
    if (Options->bUseFilterModulesFileList && (!Options->bOnlyBaseModule))
    {
        // set filtering type
        if (pOptions->bFilterModulesFileListIsExclusionList)
        {
            pApiOverride->SetModuleFilteringWay(FILTERING_WAY_NOT_SPECIFIED_MODULES);
            // load module list
            pApiOverride->AddToFiltersModuleList(pOptions->ExclusionFiltersModulesFileList);
        }
        else
        {
            pApiOverride->SetModuleFilteringWay(FILTERING_WAY_ONLY_SPECIFIED_MODULES);
            // load module list
            pApiOverride->AddToFiltersModuleList(pOptions->InclusionFiltersModulesFileList);
        }
    }

    // if command line we have to load monitoring and faking files passed as arg
    if (bNoGUI || (bCommandLine && bLoadCommandLineMonitoringAndFakingFilesNeverCalled))
        // load monitoring and faking files
        LoadCommandLineMonitoringAndFakingFiles();// Must be called only once if not bNoGui

    return TRUE;
}
BOOL SetAfterStartOptions(CApiOverride* pApiOverride)
{
    return SetAfterStartOptions(pApiOverride,pOptions);
}

//-----------------------------------------------------------------------------
// Name: CallAnalysisThread
// Object: Do or undo call analysis
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
DWORD WINAPI CallAnalysisThread(LPVOID lpParameter)
{
    UNREFERENCED_PARAMETER(lpParameter);

    if (pLogList->GetItemsCount()==0)
    {
        CleanThreadHandleIfNotExiting(&hThreadCallAnalysis);
        return 0;
    }
    // set wait mouse cursor
    SetWaitCursor(TRUE);

    // disable a new analysis
    pToolbar->EnableButton(IDC_BUTTON_ANALYSIS,FALSE);
    pLogList->Lock();// avoid list to be deleted during sorting

    if ((pToolbar->GetButtonState(IDC_BUTTON_ANALYSIS)&TBSTATE_CHECKED)!=0)
    {
        // forbid column sorting
        pListview->EnableColumnSorting(FALSE);

        // clear listview
        pListview->Clear();

        //////////////////////////////////////////////////////////////////
        // sort item by Process, next Thread, next according to CallStack
        //////////////////////////////////////////////////////////////////

        LOG_LIST_ENTRY lle;
        DWORD dwMsgLen;
        CLinkListItem* pItemMsg;
        LOG_LIST_ENTRY* pCurrentLogListEntry;
        LOG_LIST_ENTRY* pAlreadyInsertedLogListEntry;
        CLinkListItem* pLogItem;
        CLinkListItem* pItem;
        CLinkListItem* pItemProcess;
        CLinkListItem* pItemThread;
        BOOL bProcessFound;
        BOOL bThreadFound;
        /* old way using ebp parsing
        int Cnt;
        BOOL Parent;
        int delta;
        */
        CLinkList ProcessesList(sizeof(ANALYSIS_PROCESSES));
        ANALYSIS_PROCESSES* pProcess=NULL;
        ANALYSIS_THREADS* pThread=NULL;
        CLinkListItem* pItemToRemove;
        CLinkListItem* pNextItem;
        
        BOOL ParentFound;
        CLinkListSimple IncrementList;
        int Increment;
        TCHAR szMsg[MAX_PATH];
        BOOL bInserted;
        LONGLONG AlreadyInsertedTimeQuadPart;
        LONGLONG TimeQuadPart;

        CApiOverride ApiOverride;
        ApiOverride.SetMonitoringListview(pListview);

        for (pLogItem=pLogList->Head;pLogItem;pLogItem=pLogItem->NextItem)
        {
            pCurrentLogListEntry=(LOG_LIST_ENTRY*)pLogItem->ItemData;
            // if pCurrentLogListEntry is a user message
            if (pCurrentLogListEntry->Type!=ENTRY_LOG)
                continue;

            // pCurrentLogListEntry is a log

            // find process
            bProcessFound=FALSE;
            
            for (pItem=ProcessesList.Head;pItem;pItem=pItem->NextItem)
            {
                pProcess=(ANALYSIS_PROCESSES*)pItem->ItemData;
                // compare process id of the list to log process Id
                if (pProcess->ProcessId==pCurrentLogListEntry->pLog->pHookInfos->dwProcessId)
                {
                    // if id match
                    bProcessFound=TRUE;
                    break;
                }
            }

            // if not found add process
            if (!bProcessFound)
            {
                ANALYSIS_PROCESSES Ap;
                Ap.ProcessId=pCurrentLogListEntry->pLog->pHookInfos->dwProcessId;
                Ap.ThreadList=new CLinkList(sizeof(ANALYSIS_THREADS));
                if (!Ap.ThreadList)
                    // no more memory --> stop all
                    break;
                
                pItem=ProcessesList.AddItem(&Ap);
                if (!pItem)
                    // no more memory --> stop all
                    break;

                pProcess=(ANALYSIS_PROCESSES*)pItem->ItemData;
            }

            // find thread
            bThreadFound=FALSE;
            
            for (pItem=pProcess->ThreadList->Head;pItem;pItem=pItem->NextItem)
            {
                pThread=(ANALYSIS_THREADS*)pItem->ItemData;
                // compare process id of the list to log process Id
                if (pThread->ThreadId==pCurrentLogListEntry->pLog->pHookInfos->dwThreadId)
                {
                    // if id match
                    bThreadFound=TRUE;
                    break;
                }
            }
            // if not found add thread
            if (!bThreadFound)
            {
                ANALYSIS_THREADS At;
                At.ThreadId=pCurrentLogListEntry->pLog->pHookInfos->dwThreadId;
                At.LogList=new CLinkListSimple();
                if (!At.LogList)
                    // no more memory --> stop all
                    break;

                pItem=pProcess->ThreadList->AddItem(&At);
                if (!pItem)
                    // no more memory --> stop all
                    break;

                pThread=(ANALYSIS_THREADS*)pItem->ItemData;
            }
            //////////////////////////////////////////////////////////////////////////////////////////////////////
            //////////////////////////////////////////////////////////////////////////////////////////////////////
            // do time ordering.

            // here logs in pThread->LogList are originated from same process and thread
            // we have to reorder them as if you hook both kernel32|WriteFile and ntdll|NtWriteFile, 
            // as NtWriteFile ends before WriteFile, log will appears first

            // walk the list backward
            bInserted=FALSE;
            for (pItem=pThread->LogList->Tail;pItem;pItem=pItem->PreviousItem)
            {
                // get previous item in list
                pAlreadyInsertedLogListEntry=(LOG_LIST_ENTRY*)pItem->ItemData;

                // if previous item time is lower or than current, order is ok
                AlreadyInsertedTimeQuadPart=(((LONGLONG)pAlreadyInsertedLogListEntry->pLog->pHookInfos->CallTime.dwHighDateTime)<<32)
                                            +pAlreadyInsertedLogListEntry->pLog->pHookInfos->CallTime.dwLowDateTime;
                TimeQuadPart=(((LONGLONG)pCurrentLogListEntry->pLog->pHookInfos->CallTime.dwHighDateTime)<<32)
                                            +pCurrentLogListEntry->pLog->pHookInfos->CallTime.dwLowDateTime;

                if (AlreadyInsertedTimeQuadPart<=TimeQuadPart)
                {
                    // insert item
                    pThread->LogList->InsertItem(pItem,pCurrentLogListEntry);
                    bInserted=TRUE;
                    // stop order checking
                    break;
                }
            }
            if (!bInserted)
                // add log at the begin of the list
                pThread->LogList->InsertItem(NULL,pCurrentLogListEntry);

            // end of time ordering
            //////////////////////////////////////////////////////////////////////////////////////////////////////
            //////////////////////////////////////////////////////////////////////////////////////////////////////

            // if time ordering is done at log retrieval we just need to do the following
            // pThread->LogList->AddItem(pCurrentLogListEntry);
        }

        //////////////////////////////
        // add to listview
        //////////////////////////////
        Increment=0;

        // remove previous analysis message if any
        for (pItemMsg=pAnalysisInfoMessages->Head;pItemMsg;pItemMsg=pItemMsg->NextItem)
        {
            LOG_LIST_ENTRY* pLogEntry=NULL;
            pLogEntry=((LOG_LIST_ENTRY*)pItemMsg->ItemData);
            if (pLogEntry->ReportEntry.pUserMsg)
            {
                HeapFree(mhLogHeap,0,pLogEntry->ReportEntry.pUserMsg);
                pLogEntry->ReportEntry.pUserMsg=NULL;
            }
        }
        pAnalysisInfoMessages->RemoveAllItems();

        
        // for each process
        for (pItemProcess=ProcessesList.Head;pItemProcess;pItemProcess=pItemProcess->NextItem)
        {
            pProcess=(ANALYSIS_PROCESSES*)pItemProcess->ItemData;

            // if not first process
            if (pItemProcess!=ProcessesList.Head)
                // add an empty line
                pListview->AddItem(_T(""));

            // add process id to listview using a fake log entry to don't care about colors
            _stprintf(szMsg,_T("Process 0x") _T("%X"),pProcess->ProcessId);

            // fill LOG_LIST_ENTRY struct
            lle.dwId=0;
            lle.pLog=NULL;
            lle.Type=ENTRY_MSG_INFORMATION;
            // _tcsdup like to allow deletion with delete
            dwMsgLen=(DWORD)_tcslen(szMsg);
            lle.ReportEntry.pUserMsg=(TCHAR*)HeapAlloc(mhLogHeap,0,(dwMsgLen+1)*sizeof(TCHAR));
            memcpy(lle.ReportEntry.pUserMsg,szMsg,dwMsgLen*sizeof(TCHAR));
            lle.ReportEntry.pUserMsg[dwMsgLen]=0;
            // add item to LogList
            pItemMsg=pAnalysisInfoMessages->AddItem(&lle);
            if (pItemMsg)
                // don't use pApiOverride->AddLogEntry(&lle,TRUE); because lle is local memory free at the end of the function
                // use the allocated memory of link list instead
                ApiOverride.AddLogEntry((LOG_LIST_ENTRY*)pItemMsg->ItemData,TRUE);


            if (pProcess->ThreadList)
            {
                // for each thread
                for (pItemThread=pProcess->ThreadList->Head;pItemThread;pItemThread=pItemThread->NextItem)
                {
                    pThread=(ANALYSIS_THREADS*)pItemThread->ItemData;

                    // if not first thread
                    if (pItemThread!=pProcess->ThreadList->Head)
                        // add an empty line
                        pListview->AddItem(_T(""));

                    // add thread id to listview using a fake log entry to don't care about colors
                    _stprintf(szMsg,_T("Thread 0x") _T("%X"),pThread->ThreadId);

                    // fill LOG_LIST_ENTRY struct
                    lle.dwId=0;
                    lle.pLog=NULL;
                    lle.Type=ENTRY_MSG_INFORMATION;
                    // _tcsdup like to allow deletion with delete
                    dwMsgLen=(DWORD)_tcslen(szMsg);
                    lle.ReportEntry.pUserMsg=(TCHAR*)HeapAlloc(mhLogHeap,0,(dwMsgLen+1)*sizeof(TCHAR));
                    memcpy(lle.ReportEntry.pUserMsg,szMsg,dwMsgLen*sizeof(TCHAR));
                    lle.ReportEntry.pUserMsg[dwMsgLen]=0;
                    // add item to LogList
                    pItemMsg=pAnalysisInfoMessages->AddItem(&lle);
                    if (pItemMsg)
                        // don't use pApiOverride->AddLogEntry(&lle,TRUE); because lle is local memory free at the end of the function
                        // use the allocated memory of link list instead
                        ApiOverride.AddLogEntry((LOG_LIST_ENTRY*)pItemMsg->ItemData,TRUE);


                    if (pThread->LogList)
                    {
                        // clear Increment List
                        IncrementList.RemoveAllItems();

                        // for each item of list
                        for (pLogItem=pThread->LogList->Head;pLogItem;pLogItem=pLogItem->NextItem)
                        {
                            pCurrentLogListEntry=(LOG_LIST_ENTRY*)pLogItem->ItemData;

                            ////////////////////////////////////////////////
                            // add log with increment according to its stack
                            ////////////////////////////////////////////////

                            ParentFound=FALSE;

                            /* new log way using per thread parent call time */
                            // if no logged parent
                            if (  (pCurrentLogListEntry->pLog->pHookInfos->FirstHookedParentCallTime.dwHighDateTime==0)
                                  && (pCurrentLogListEntry->pLog->pHookInfos->FirstHookedParentCallTime.dwLowDateTime==0)
                               )
                            {
                                goto AfterIncrementListBackwardWalk;
                            }
                            /* end of new log way using per thread parent call time */

                            // walk the IncrementList backward to get increment value
                            for (pItem=IncrementList.Tail;pItem;pItem=pItem->PreviousItem)
                            {
                                // get previous item in list
                                pAlreadyInsertedLogListEntry=(LOG_LIST_ENTRY*)pItem->ItemData;

                                /* old way using ebp parsing. Could fail if an hooked caller doesn't push ebp
                                   let for intellectual purpose only


                                if (pAlreadyInsertedLogListEntry->pLog->pHookInfos->CallStackSize<pCurrentLogListEntry->pLog->pHookInfos->CallStackSize)
                                {

                                    if (pAlreadyInsertedLogListEntry->pLog->pHookInfos->CallStackSize==0)
                                    {
                                        if (pAlreadyInsertedLogListEntry->pLog->pHookInfos->pOriginAddress
                                            ==pCurrentLogListEntry->pLog->CallSackInfoArray[pCurrentLogListEntry->pLog->pHookInfos->CallStackSize-1].Address)
                                        {
                                            // pAlreadyInsertedLogListEntry is the only one parent
                                            ParentFound=TRUE;
                                            IncrementList.RemoveAllItems();
                                            IncrementList.AddItem(pAlreadyInsertedLogListEntry);
                                            Increment=1;
                                            // add current log to IncrementList
                                            IncrementList.AddItem(pCurrentLogListEntry);
                                            break;// go out of IncrementList backward walk
                                        }
                                    }
                                    else
                                    {
                                        // pAlreadyInsertedLogListEntry could be a parent call of pCurrentLogListEntry
                                        // check it by checking each element of the stack

                                        Parent=TRUE;
                                        // check address in reverse order as the probability of same address at the begin of stack is high
                                        // remember stack array is already in reverse order
                                        // compute index delta in stack array
                                        delta=pCurrentLogListEntry->pLog->pHookInfos->CallStackSize-pAlreadyInsertedLogListEntry->pLog->pHookInfos->CallStackSize;

                                        // check pAlreadyInsertedLogListEntry return value first
                                        // Warning : this checking can fail if pAlreadyInsertedLogListEntry function hasn't pushed ebp
                                        if (pCurrentLogListEntry->pLog->CallSackInfoArray[delta-1].Address!=pAlreadyInsertedLogListEntry->pLog->pHookInfos->pOriginAddress)
                                        {
                                            // not a parent call --> continue IncrementList backward walk
                                            continue;
                                        }
                                        for (Cnt=0;Cnt<pAlreadyInsertedLogListEntry->pLog->pHookInfos->CallStackSize;Cnt++)
                                        {
                                            if (pCurrentLogListEntry->pLog->CallSackInfoArray[Cnt+delta].Address!=
                                                pAlreadyInsertedLogListEntry->pLog->CallSackInfoArray[Cnt].Address)
                                            {
                                                Parent=FALSE;
                                                break;
                                            }
                                        }
                                       if (!Parent)
                                            // not a parent call --> continue IncrementList backward walk
                                            continue;
                                        end of old way using ebp parsing */ 

                                        /* new log way using per thread parent call time */
                                        if ( (pCurrentLogListEntry->pLog->pHookInfos->FirstHookedParentCallTime.dwHighDateTime!=pAlreadyInsertedLogListEntry->pLog->pHookInfos->CallTime.dwHighDateTime)
                                             ||(pCurrentLogListEntry->pLog->pHookInfos->FirstHookedParentCallTime.dwLowDateTime!=pAlreadyInsertedLogListEntry->pLog->pHookInfos->CallTime.dwLowDateTime)
                                           )
                                           continue;
                                        /* end of new log way using per thread parent call time */

                                        ParentFound=TRUE;

                                        // remove item in increment list after parent item
                                        
                                        for (pItemToRemove=pItem->NextItem;pItemToRemove;pItemToRemove=pNextItem)
                                        {
                                            // store next item
                                            pNextItem=pItemToRemove->NextItem;
                                            // remove current item
                                            IncrementList.RemoveItem(pItemToRemove);
                                        }


                                        // get increment value
                                        Increment=IncrementList.GetItemsCount();

                                        // add current log to IncrementList
                                        IncrementList.AddItem(pCurrentLogListEntry);

                                        break;// go out of IncrementList backward walk
                                /* old way using ebp parsing
                                    }
                                }
                                end of old way using ebp parsing */
                            }
AfterIncrementListBackwardWalk:
                            // if item has no parent (parent may be removed from list)
                            if (!ParentFound)
                            {
                                // increment is null
                                Increment=0;

                                // empty increment list
                                IncrementList.RemoveAllItems();

                                // add current log to IncrementList
                                IncrementList.AddItem(pCurrentLogListEntry);
                            }

                            // add log to listview
                            ApiOverride.AddLogEntry(pCurrentLogListEntry,TRUE,Increment);
                        }
                    }
                }
            }
        }

        //////////////////////////////
        // free memory
        //////////////////////////////
        
        // for each process
        for (pItemProcess=ProcessesList.Head;pItemProcess;pItemProcess=pItemProcess->NextItem)
        {
            pProcess=(ANALYSIS_PROCESSES*)pItemProcess->ItemData;
            if (pProcess->ThreadList)
            {
                
                // for each thread
                for (pItemThread=pProcess->ThreadList->Head;pItemThread;pItemThread=pItemThread->NextItem)
                {
                    pThread=(ANALYSIS_THREADS*)pItemThread->ItemData;
                    if (pThread->LogList)
                        // remove log list
                        delete pThread->LogList;
                }
                
                // remove thread list
                delete pProcess->ThreadList;
            }
        }
    }
    else
    {
        // allow column sorting again
        pListview->EnableColumnSorting(TRUE);

        // undo analysis
        // to do it, just remove items from listview and add them in order of pLogList
        pListview->Clear();

        CLinkListItem* pItem;
        
        PLOG_LIST_ENTRY pLogListEntry;

        CApiOverride ApiOverride;
        ApiOverride.SetMonitoringListview(pListview);

        for(pItem=pLogList->Head;pItem;pItem=pItem->NextItem)
        {
            if (bExit)
                break;

            pLogListEntry=(PLOG_LIST_ENTRY)pItem->ItemData;

            // add items of list to list view
            ApiOverride.AddLogEntry(pLogListEntry,TRUE);
        }
    }

    if (!bExit)
    {
        // restore cursor
        SetWaitCursor(FALSE);

        // enable loading
        pToolbar->EnableButton(IDC_BUTTON_ANALYSIS,TRUE);
    }

    pLogList->Unlock();
    CleanThreadHandleIfNotExiting(&hThreadCallAnalysis);
    return 0;
}

//-----------------------------------------------------------------------------
// Name: CallAnalysis
// Object: do or undo call analysis
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void CallAnalysis()
{
    hThreadCallAnalysis = ::CreateThread(0,0,CallAnalysisThread,0,0,0);
}

//-----------------------------------------------------------------------------
// Name: GetApiOverrideObject
// Object: let user select process with which he wants to interact. 
//         Auto select if only one process hooked
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
CApiOverride* GetApiOverrideObject()
{
    CApiOverride* pApiOverride=NULL;
    DWORD NbHookedProcesses=pApiOverrideList->GetItemsCount();
    if (NbHookedProcesses==0)
        return NULL;
    else if (NbHookedProcesses==1)
        return (CApiOverride*)pApiOverrideList->Head->ItemData;
    else
    {
        pApiOverride=CSelectHookedProcess::ShowDialog(mhInstance,mhWndDialog);
    }

    return pApiOverride;
}

//-----------------------------------------------------------------------------
// Name: Dump
// Object: Query Dump for hooked processes
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void Dump()
{
    CApiOverride* pApiOverride=GetApiOverrideObject();
    if (!pApiOverride)
        return;
    pApiOverride->Dump();
}

//-----------------------------------------------------------------------------
// Name: CompareLogs
// Object: compare the two first selected log items
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void CompareLogs()
{
    LOG_LIST_ENTRY* pLogEntry=NULL;

    LOG_ENTRY* pLog1=NULL;
    LOG_ENTRY* pLog2=NULL;

    int cnt;
    int NbItems;
    int NbFound;

    // for each item in plistview
    NbItems=pListview->GetItemCount();
    NbFound=0;
    for (cnt=0;cnt<NbItems;cnt++)
    {
        // if item is not selected
        if (!pListview->IsItemSelected(cnt))
            // go to next item
            continue;

        // else item is selected

        // search for next matching item in listview
        if(!pListview->GetItemUserData(cnt,(LPVOID*)(&pLogEntry)))
            continue;
        if (pLogEntry==0)
            continue;
        if (IsBadReadPtr(pLogEntry,sizeof(LOG_LIST_ENTRY)))
            continue;
        if (pLogEntry->Type!=ENTRY_LOG)
            continue;
        if (IsBadReadPtr(pLogEntry->pLog,sizeof(LOG_ENTRY)))
        {
            // remove it from listview
            pListview->SetItemUserData(cnt,0);
            pListview->RemoveItem(cnt);
            continue;
        }

        if (NbFound==0)
            pLog1=pLogEntry->pLog;
        else
            pLog2=pLogEntry->pLog;
        NbFound++;
        if (NbFound>=2)
            break;
    }

    if (NbFound!=2)
    {
        MessageBox(mhWndDialog,_T("You have to select two logs to make a comparison !"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // if func to be compare are not the same
    if (_tcsicmp(pLog1->pszApiName,pLog2->pszApiName)
        ||_tcsicmp(pLog1->pszModuleName,pLog2->pszModuleName))
    {
        TCHAR Msg[3*MAX_PATH];
        _stprintf(Msg,
            _T("You are going to compare not identical function\r\n(%s and %s)\r\nDo you want to continue ?"),
            pLog1->pszApiName,
            pLog2->pszApiName);
        if (MessageBox(mhWndDialog,Msg,_T("Information"),MB_YESNO|MB_ICONINFORMATION|MB_TOPMOST)!=IDYES)
            return;
    }

    // make comparison
    CCompareLogs::ShowDialog(mhInstance,mhWndDialog,pLog1,pLog2);
}

//-----------------------------------------------------------------------------
// Name: MonitoringWizard
// Object: show monitoring wizard window, and if user use wizard, unload 
//              already loaded files, and load those specified by wizard
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void MonitoringWizard()
{
    CloseHandle(CreateThread(NULL,0,MonitoringWizard,NULL,0,NULL));
}
DWORD WINAPI MonitoringWizard(LPVOID lpParameter)
{
    UNREFERENCED_PARAMETER (lpParameter);

    CMonitoringWizard pobj(CMonitoringWizard::MonitoringWizardType_API);
    if (pobj.ShowDialog(mhInstance,mhWndDialog)!=MONITORING_WIZARD_DLG_RES_OK)
        return 0;
    // else we have to unload already loaded file and load the new ones

    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MONITORING_WIZARD),FALSE);

    TCHAR* pszFileToLoad;
    CLinkListItem* pItem;
       
    ///////////////////////////////
    // unload already loaded files
    ///////////////////////////////
    UnloadMonitoringFileThread((LPVOID)1);
    
    ///////////////////////////////
    // load new files
    ///////////////////////////////
    
    for(pItem=pobj.pFileToLoadList->Head;pItem;pItem=pItem->NextItem)
    {
        // get file to load
        pszFileToLoad=(TCHAR*)pItem->ItemData;
        LoadMonitoringFile(pszFileToLoad);
    }
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MONITORING_WIZARD),TRUE);

    return 0;
}
//-----------------------------------------------------------------------------
// Name: EditModulesFiltersList
// Object: Allow to edit exclusion list
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void EditModulesFiltersList()
{
    TCHAR psz[MAX_PATH];
    // retrieve current exclusion file filter
    GetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,psz,MAX_PATH);

    // check if full path is given
    if (!_tcschr(psz,'\\'))// search for a \ 
    {
        TCHAR pszPath[MAX_PATH];
        CStdFileOperations::GetAppPath(pszPath,MAX_PATH);
        _tcscat(pszPath,psz);
        _tcscpy(psz,pszPath);
    }

    // check if file exists
    if(!CStdFileOperations::DoesFileExists(psz))
    {
        TCHAR pszMsg[2*MAX_PATH];
        _sntprintf(pszMsg,2*MAX_PATH,_T("File %s not found"),psz);
        MessageBox(mhWndDialog,pszMsg,_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // launch default .txt editor
    if (((int)ShellExecute(NULL,_T("edit"),psz,NULL,NULL,SW_SHOWNORMAL))<33)
    {
        MessageBox(mhWndDialog,_T("Error launching default editor application"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
    }
}

//-----------------------------------------------------------------------------
// Name: Help
// Object: show help file
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void Help()
{
    TCHAR psz[MAX_PATH];
    _tcscpy(psz,pszApplicationPath);
    _tcscat(psz,HELP_FILE);
    if (((int)ShellExecute(NULL,_T("open"),psz,NULL,NULL,SW_SHOWNORMAL))<33)
    {
        _tcscpy(psz,_T("Error opening help file "));
        _tcscat(psz,HELP_FILE);
        MessageBox(NULL,psz,_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
    }
}
//-----------------------------------------------------------------------------
// Name: MonitoringFileGenerator
// Object: launch monitoring file generator
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void MonitoringFileGenerator()
{
    TCHAR psz[MAX_PATH];
    _tcscpy(psz,pszApplicationPath);
    _tcscat(psz,MONITORING_FILE_BUILDER_APP_NAME);
    if (((int)ShellExecute(NULL,_T("open"),psz,NULL,NULL,SW_SHOWNORMAL))<33)
    {
        _tcscpy(psz,_T("Error launching application "));
        _tcscat(psz,MONITORING_FILE_BUILDER_APP_NAME);
        MessageBox(NULL,psz,_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
    }
}
//-----------------------------------------------------------------------------
// Name: MakeDonation
// Object: make donation
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void MakeDonation()
{
    BOOL bEuroCurrency=FALSE;
    HKEY hKey;
    wchar_t pszCurrency[2];
    DWORD Size=2*sizeof(wchar_t);
    memset(pszCurrency,0,Size);
    TCHAR pszMsg[3*MAX_PATH];

    // check that HKEY_CURRENT_USER\Control Panel\International\sCurrency contains the euro symbol
    // retrieve it in unicode to be quite sure of the money symbol
    if (RegOpenKeyEx(HKEY_CURRENT_USER,_T("Control Panel\\International"),0,KEY_QUERY_VALUE,&hKey)==ERROR_SUCCESS)
    {
        // use unicode version only to make string compare
        if (RegQueryValueExW(hKey,L"sCurrency",NULL,NULL,(LPBYTE)pszCurrency,&Size)==ERROR_SUCCESS)
        {
            if (wcscmp(pszCurrency,L"€")==0)
                bEuroCurrency=TRUE;
        }
        // close open key
        RegCloseKey(hKey);
    }
    // yes, you can do it if u don't like freeware and open sources soft
    // but if you make it, don't blame me for not releasing sources anymore
    _tcscpy(pszMsg,_T("https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=jacquelin.potier@free.fr")
                   _T("&currency_code=USD&lc=EN&country=US")
                   _T("&item_name=Donation%20for%20Winapioverride&return=http://jacquelin.potier.free.fr/winapioverride32/"));
    // in case of euro monetary symbol
    if (bEuroCurrency)
        // add it to link
        _tcscat(pszMsg,_T("&currency_code=EUR"));

    // open donation web page
    if (((int)ShellExecute(NULL,_T("open"),pszMsg,NULL,NULL,SW_SHOWNORMAL))<33)
        // display error msg in case of failure
        MessageBox(NULL,_T("Error Opening default browser. You can make a donation going to ")
                        _T("http://jacquelin.potier.free.fr"),
                        _T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
}
//-----------------------------------------------------------------------------
// Name: ReloadMonitoring
// Object: Reload monitoring files
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ReloadMonitoring()
{
    // create a new thread to allow unload of monitoring/overriding of DlgProc and SendMessage
    hThreadReloadMonitoring=::CreateThread(NULL,0,ReloadMonitoring,NULL,0,NULL);
}
DWORD WINAPI ReloadMonitoring(LPVOID lpParameter)
{
    UNREFERENCED_PARAMETER(lpParameter);

    // save current list if no item in current list, last loaded items will be loaded
    SaveMonitoringListContent();

    // if empty list
    if (pOptions->MonitoringFilesList->GetItemsCount()==0)
    {
        MessageBox(mhWndDialog,_T("No files were previously loaded"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        CleanThreadHandleIfNotExiting(&hThreadReloadMonitoring);
        return 0;
    }

    // disable refresh button during the reload
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MONITORING_RELOAD),FALSE);

    // unload already loaded files
    UnloadMonitoringFileThread((LPVOID)1);

    // for each item of list reload monitoring file
    CLinkListItem* pItemp;
    
    pOptions->MonitoringFilesList->Lock();
    for(pItemp=pOptions->MonitoringFilesList->Head;pItemp;pItemp=pItemp->NextItem)
    {
        LoadMonitoringFile((TCHAR*)pItemp->ItemData);
    }
    pOptions->MonitoringFilesList->Unlock();

    // enable reload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MONITORING_RELOAD),TRUE);

    CleanThreadHandleIfNotExiting(&hThreadReloadMonitoring);
    return 0;
}
//-----------------------------------------------------------------------------
// Name: ReloadOverriding
// Object: reload overriding dll
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ReloadOverriding()
{
    // create a new thread to allow unload of monitoring/overriding of DlgProc and SendMessage
    hThreadReloadOverriding=::CreateThread(NULL,0,ReloadOverriding,NULL,0,NULL);
}
DWORD WINAPI ReloadOverriding(LPVOID lpParameter)
{
    UNREFERENCED_PARAMETER(lpParameter);

    // save current list if no item in current list, last loaded items will be loaded
    SaveOverridingListContent();

    // if empty list
    if (pOptions->OverridingDllList->GetItemsCount()==0)
    {
        MessageBox(mhWndDialog,_T("No dll were previously loaded"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        CleanThreadHandleIfNotExiting(&hThreadReloadOverriding);
        return 0;
    }

    // disable refresh button during the reload
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_FAKING_RELOAD),FALSE);

    // unload already loaded files
    UnloadOverridingFileThread((LPVOID)1);

    // for each item of list reload dll
    CLinkListItem* pItemp;
    pOptions->OverridingDllList->Lock();
    for(pItemp=pOptions->OverridingDllList->Head;pItemp;pItemp=pItemp->NextItem)
    {
        LoadOverridingFile((TCHAR*)pItemp->ItemData);
    }
    pOptions->OverridingDllList->Unlock();

    // enable reload button
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_FAKING_RELOAD),TRUE);
    CleanThreadHandleIfNotExiting(&hThreadReloadOverriding);
    return 0;
}
//-----------------------------------------------------------------------------
// Name: UpdateModulesFiltersList
// Object: update modules exclusion list
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void UpdateModulesFiltersList()
{
    CApiOverride* pApiOverride;
    CLinkListItem* pItem;

    // if we currently don't use list do nothing
    if (!pOptions->bUseFilterModulesFileList)
        return;

    // update the filtering way
    pOptions->bFilterModulesFileListIsExclusionList=(IsDlgButtonChecked(mhWndDialog,IDC_RADIO_EXCLUSION_LIST)==BST_CHECKED);

    // update module list
    if (pOptions->bFilterModulesFileListIsExclusionList)
        GetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->ExclusionFiltersModulesFileList,MAX_PATH);
    else
        GetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pOptions->InclusionFiltersModulesFileList,MAX_PATH);

    
    // loop through pApiOverrideList (hooked processes list)
    pApiOverrideList->Lock(TRUE);
    for (pItem=pApiOverrideList->Head;pItem;pItem=pItem->NextItem)
    {
        pApiOverride=(CApiOverride*)pItem->ItemData;

        // Clear not logged module list
        pApiOverride->ClearFiltersModuleList();

        // set filtering type
        if (pOptions->bFilterModulesFileListIsExclusionList)
        {
            pApiOverride->SetModuleFilteringWay(FILTERING_WAY_NOT_SPECIFIED_MODULES);
            // load module list
            pApiOverride->AddToFiltersModuleList(pOptions->ExclusionFiltersModulesFileList);
        }
        else
        {
            pApiOverride->SetModuleFilteringWay(FILTERING_WAY_ONLY_SPECIFIED_MODULES);
            // load module list
            pApiOverride->AddToFiltersModuleList(pOptions->InclusionFiltersModulesFileList);
        }
    }
    pApiOverrideList->Unlock();
}

//-----------------------------------------------------------------------------
// Name: CallBackReportMessage
// Object: call back for injected dll report message
//          Notice we are forced to use this way as multiple CApiOverride object can exist together
//          report message must transit by this main to get a unique log entry identifier
// Parameters :
//     in  : tagReportMessageType ReportMessageType : type of message (information, error , warning)
//           TCHAR* pszReportMessage : message to display
//           UserParam : pointer to an CApiOverride object for which the message applies
//     return : 
//-----------------------------------------------------------------------------
void STDMETHODCALLTYPE CallBackReportMessage(tagReportMessageType ReportMessageType,TCHAR* pszReportMessage,FILETIME FileTime,LPVOID UserParam)
{
    if (IsBadReadPtr(UserParam,sizeof(CApiOverride)))
        return;
    ReportMessage((CApiOverride*)UserParam,pszReportMessage,(tagMsgTypes)ReportMessageType,FileTime);
}

void STDMETHODCALLTYPE CallBackOverridingDllQuery(TCHAR* PluginName,PVOID MessageId,PBYTE pMsg,SIZE_T MsgSize,PVOID UserParam)
{
    if (IsBadReadPtr(UserParam,sizeof(CApiOverride)))
        return;
    if (!pPluginManager)
        return;
    pPluginManager->OnOverridingDllQuery((CApiOverride*)UserParam,PluginName,MessageId,pMsg,MsgSize);
}
//-----------------------------------------------------------------------------
// Name: ReportMessage
// Object: add an information message into the listview and log list
// Parameters :
//     in  : CApiOverride* pApiOverride : Api override object for which the message applies
//           TCHAR* pszMsg : message to display
//           tagMsgTypes MsgType : type of message (information, error , warning)
//     return : 
//-----------------------------------------------------------------------------
void ReportMessage(CApiOverride* pApiOverride,TCHAR* pszMsg, tagMsgTypes MsgType)
{
    FILETIME ft;
    // get UTC time
    GetSystemTimeAsFileTime(&ft);
    // convert UTC time to local filetime
    FileTimeToLocalFileTime(&ft,&ft);
    ReportMessage(pApiOverride,pszMsg,MsgType,ft);
}

//-----------------------------------------------------------------------------
// Name: ReportMessage
// Object: add an information message into the listview and log list
// Parameters :
//     in  : CApiOverride* pApiOverride : Api override object for which the message applies
//           TCHAR* pszMsg : message to display
//           tagMsgTypes MsgType : type of message (information, error , warning)
//           FILETIME Time : report filetime
//     return : 
//-----------------------------------------------------------------------------
void ReportMessage(CApiOverride* pApiOverride,TCHAR* pszMsg, tagMsgTypes MsgType,FILETIME Time)
{
    LOG_LIST_ENTRY lle;
    DWORD dwMsgLen;
    CLinkListItem* pItem;

    // increase log id
    dwLogId++;

    // fill LOG_LIST_ENTRY struct
    lle.dwId=dwLogId;
    switch (MsgType)
    {
    case MSG_WARNING:
        lle.Type=ENTRY_MSG_WARNING;
        break;
    case MSG_ERROR:
        lle.Type=ENTRY_MSG_ERROR;
        break;
    case MSG_EXCEPTION:
        lle.Type=ENTRY_MSG_EXCEPTION;
        break;
    case MSG_INFORMATION:
    default:
        lle.Type=ENTRY_MSG_INFORMATION;
        break;
    }
    // _tcsdup like to allow deletion with delete
    dwMsgLen=(DWORD)_tcslen(pszMsg);
    lle.ReportEntry.pUserMsg=(TCHAR*)HeapAlloc(mhLogHeap,0,(dwMsgLen+1)*sizeof(TCHAR));
    memcpy(lle.ReportEntry.pUserMsg,pszMsg,dwMsgLen*sizeof(TCHAR));
    lle.ReportEntry.pUserMsg[dwMsgLen]=0;
    lle.ReportEntry.ReportTime=Time;

    pLogList->Lock();// avoid list to be clear when filling list view

    // add item to LogList
    pItem=pLogList->AddItem(&lle,TRUE);

    if (pItem)
    {
        if (!bNoGUI)
        {
            // don't use pApiOverride->AddLogEntry(&lle,TRUE); because lle is local memory free at the end of the function
            // use the allocated memory of link list instead
            pApiOverride->AddLogEntry((LOG_LIST_ENTRY*)pItem->ItemData,TRUE);
        }
    }

    pLogList->Unlock();
}

//-----------------------------------------------------------------------------
// Name: ReportMessage
// Object: add an information message into the listview and log list
//          use only when no ApiOverride object is associated to message
//          (because this function allocate and free memory --> slowest)
// Parameters :
//     in  : TCHAR* pszMsg : message to display
//     out : tagMsgTypes MsgType : type of message (inforamtion, error , warning)
//     return : 
//-----------------------------------------------------------------------------
void ReportMessage(TCHAR* pszMsg, tagMsgTypes MsgType)
{
    CApiOverride ApiOverride;
    ApiOverride.SetMonitoringListview(pListview);
    ReportMessage(&ApiOverride,pszMsg,MsgType);
}

//-----------------------------------------------------------------------------
// Name: CallbackMonitoringLog
// Object: add an information message into the listview
// Parameters :
//     in  : LOG_ENTRY* pLog
//           PVOID pUserParam : user parameter used here to store the calling CApiOverride object
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void STDMETHODCALLTYPE CallbackMonitoringLog(LOG_ENTRY* pLog,PVOID pUserParam)
{
    LOG_LIST_ENTRY lle;
    CLinkListItem* pItem;
    // increase log id
    dwLogId++;

    // fill LOG_LIST_ENTRY struct
    lle.dwId=dwLogId;
    lle.pLog=pLog;
    lle.Type=ENTRY_LOG;

    // avoid item to be deleted will being added to listview
    pLogList->Lock();

    // add item to LogList
    pItem=pLogList->AddItem(&lle,TRUE);

    // add item to listview
    if (pItem)
    {
        if (!bNoGUI)
        {
            // don't use pApiOverride->AddLogEntry(&lle,TRUE); because lle is local memory free at the end of the function
            // use the allocated memory of link list instead
            ((CApiOverride*)pUserParam)->AddLogEntry((LOG_LIST_ENTRY*)pItem->ItemData,TRUE);
        }
    }
    pLogList->Unlock();
}

//-----------------------------------------------------------------------------
// Name: ListviewPopUpMenuItemClickCallback
// Object: static Call back for list view popup menu item click
// Parameters :
//     in  : int MenuID : Id of menu clicked
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ListviewPopUpMenuItemClickCallback(UINT MenuID,LPVOID UserParam)
{
    // as multiple CApiOverride can use a unique CListview,
    // and all object can be destroyed, but CListview remains active,
    // we can't play directly with MenuID.
    // so here we retrieve menu text to know static action to execute
    TCHAR psz[MAX_PATH];
    CListview* pListview=(CListview*)UserParam;
    if (!(pListview->pPopUpMenu->GetText(MenuID,psz,MAX_PATH)))
            return;

    // find clicked menu item (check menu item name)
    if (_tcsicmp(psz,MENU_GET_NONREBASED_VIRTUAL_ADDRESS)==0)
        GetUnrebasedVirtualAddress(pListview);
    else if(_tcsicmp(psz,MENU_SHOW_GETLASTERROR_MESSAGE)==0)
    {
        ShowLastErrorMessage(pListview);
    }
    else if(_tcsicmp(psz,MENU_SHOW_RETURN_VALUE_ERROR_MESSAGE)==0)
    {
        ShowReturnErrorMessage(pListview);
    }
    else if(_tcsicmp(psz,MENU_ONLINE_MSDN_HELP)==0)
    {
        ShowMSDNOnlineHelp(pListview);
    }
    else if(_tcsicmp(psz,MENU_GOOGLE_FUNC_NAME)==0)
    {
        GoogleFuncName(pListview);
    }
    else if(_tcsicmp(psz,MENU_COPY_FUNC_NAME)==0)
    {
        CopyFuncName(pListview);
    }
    else if (_tcsicmp(psz,MENU_CLEAR)==0)
    {
        ClearLogs(FALSE);
    }
    else if (_tcsicmp(psz,MENU_REMOVE_SELECTED_ITEMS)==0)
    {
        RemoveSelectedLogs();
    }
    else if (_tcsicmp(psz,MENU_INVERT_SELECTION)==0)
    {
        pListview->InvertSelection();
    }
    else if (_tcsicmp(psz,MENU_GOTO_NEXT_FAILURE)==0)
    {
        FindNextFailure();
    }
    else if (_tcsicmp(psz,MENU_GOTO_PREVIOUS_FAILURE)==0)
    {
        FindPreviousFailure();
    }
    else if (_tcsicmp(psz,MENU_SAVE_ALL)==0)
    {
        SaveLogs();
    }
}


//-----------------------------------------------------------------------------
// Name: GoogleFuncName
// Object: Show google online result for first selected item of monitoring listview
// Parameters :
//     in  : CListview* pListview : log listview object
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void GoogleFuncName(CListview* pListview)
{

    TCHAR psz[MAX_PATH];
    BOOL bDefault=FALSE;
    // retrieve selected item in listview
    int Index=-1;
    if (pListview)
        Index=pListview->GetSelectedIndex();
    if (Index==-1)
        bDefault=TRUE;
    else
    {
        // retrieve function name
        pListview->GetItemText(Index,CApiOverride::ColumnsIndexAPIName,psz,MAX_PATH);

        if (*psz==0)
            bDefault=TRUE;

    }
    if (bDefault)
    {
        // if not found: default online help
        if (((int)ShellExecute(NULL,_T("open"),GOOGLE_ONLINE_DEFAULT_LINK,NULL,NULL,SW_SHOWNORMAL))<33)
        {
            MessageBox(mhWndDialog,_T("Error launching default browser application"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        }
        return;
    }

    // check func name to remove ansi / unicode terminator (A or W)
    size_t FuncNameSize=_tcslen(psz);
    if (FuncNameSize>2)
    {
        if (((psz[FuncNameSize-1]=='A')||(psz[FuncNameSize-1]=='W'))
            // && isLowerCase(psz[FuncNameSize-1])
            && ((psz[FuncNameSize-2]>='a')&&(psz[FuncNameSize-2]<='z'))
            )
        {
            // remove A or W
            psz[FuncNameSize-1]=0;
        }
    }

    // make url
    TCHAR pszURL[MAX_PATH];
    _stprintf(pszURL,GOOGLE_ONLINE_LINK,psz);

    if (((int)ShellExecute(NULL,_T("open"),pszURL,NULL,NULL,SW_SHOWNORMAL))<33)
    {
        MessageBox(mhWndDialog,_T("Error launching default browser application"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
    }

}
//-----------------------------------------------------------------------------
// Name: ShowMSDNOnlineHelp
// Object: Show MSDN online help for first selected item of monitoring listview
// Parameters :
//     in  : CListview* pListview : log listview object
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void CopyFuncName(CListview* plistview)
{
    TCHAR psz[MAX_PATH];
    // retrieve selected item in listview
    int Index=-1;
    if (plistview)
        Index=plistview->GetSelectedIndex();
    if (Index==-1)
        return;
    else
    {
        // retrieve function name
        plistview->GetItemText(Index,CApiOverride::ColumnsIndexAPIName,psz,MAX_PATH);

        if (*psz==0)
            return;

    }
    CClipboard::CopyToClipboard(plistview->GetControlHandle(),psz);
}
//-----------------------------------------------------------------------------
// Name: ShowMSDNOnlineHelp
// Object: Show MSDN online help for first selected item of monitoring listview
// Parameters :
//     in  : CListview* pListview : log listview object
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ShowMSDNOnlineHelp(CListview* pListview)
{
    TCHAR psz[MAX_PATH];
    BOOL bDefaultMSDN=FALSE;
    // retrieve selected item in listview
    int Index=-1;
    if (pListview)
        Index=pListview->GetSelectedIndex();
    if (Index==-1)
        bDefaultMSDN=TRUE;
    else
    {
        // retrieve function name
        pListview->GetItemText(Index,CApiOverride::ColumnsIndexAPIName,psz,MAX_PATH);

        if (*psz==0)
            bDefaultMSDN=TRUE;

    }
    if (bDefaultMSDN)
    {
        // if not found: default online help
        if (((int)ShellExecute(NULL,_T("open"),MSDN_ONLINE_HELP_DEFAULT_LINK,NULL,NULL,SW_SHOWNORMAL))<33)
        {
            MessageBox(mhWndDialog,_T("Error launching default browser application"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        }
        return;
    }

    // check func name to remove ansi / unicode terminator (A or W)
    size_t FuncNameSize=_tcslen(psz);
    if (FuncNameSize>2)
    {
        if (((psz[FuncNameSize-1]=='A')||(psz[FuncNameSize-1]=='W'))
            // && isLowerCase(psz[FuncNameSize-1])
            && ((psz[FuncNameSize-2]>='a')&&(psz[FuncNameSize-2]<='z'))
            )
        {
            // remove A or W
            psz[FuncNameSize-1]=0;
        }
    }

    // make url
    TCHAR pszURL[MAX_PATH];
    _stprintf(pszURL,MSDN_ONLINE_HELP_LINK,psz);

    if (((int)ShellExecute(NULL,_T("open"),pszURL,NULL,NULL,SW_SHOWNORMAL))<33)
    {
        MessageBox(mhWndDialog,_T("Error launching default browser application"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
    }
}


//-----------------------------------------------------------------------------
// Name: ShowReturnErrorMessage
// Object: Show error message for return value
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ShowReturnErrorMessage(CListview* pListview)
{
    TCHAR psz[MAX_PATH];
    DWORD dwError=0;
    // retrieve selected item in listview
    int Index=-1;
    if (pListview)
        Index=pListview->GetSelectedIndex();
    if (Index==-1)
    {
        // if not found
        MessageBox(mhWndDialog,_T("No item selected"),_T("WinAPIOverride"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // retrieve last error value
    pListview->GetItemText(Index,CApiOverride::ColumnsIndexReturnValue,psz,MAX_PATH);
    if (*psz==0)
    {
        MessageBox(mhWndDialog,_T("No returned value for the selected item."),_T("WinAPIOverride"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }
    _stscanf(psz,_T("0x%X"),&dwError);

    // show error message corresponding to error value
    if (!CAPIError::ShowError(dwError))
        MessageBox(mhWndDialog,_T("No error message associated with the returned value."),_T("WinAPIOverride"),MB_OK|MB_ICONERROR|MB_TOPMOST);

}
//-----------------------------------------------------------------------------
// Name: ShowLastErrorMessage
// Object: Show last error message for first selected item of monitoring listview
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ShowLastErrorMessage(CListview* pListview)
{
    TCHAR psz[MAX_PATH];
    DWORD dwError=0;
    // retrieve selected item in listview
    int Index=-1;
    if (pListview)
        Index=pListview->GetSelectedIndex();
    if (Index==-1)
    {
        // if not found
        MessageBox(mhWndDialog,_T("No item selected"),_T("WinAPIOverride"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }
    
    // retrieve last error value
    pListview->GetItemText(Index,CApiOverride::ColumnsIndexLastError,psz,MAX_PATH);
    if (*psz==0)
    {
        MessageBox(mhWndDialog,_T("No last error value for the selected item."),_T("WinAPIOverride"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }
    _stscanf(psz,_T("0x%X"),&dwError);

    // show error message corresponding to error value
    CAPIError::ShowError(dwError);

}

//-----------------------------------------------------------------------------
// Name: GetUnrebasedVirtualAddress
// Object: Get the non relocated virtual address from dll 
//          that means (RVA+ImageBase) instead of (RVA+rebased dll base address)
// Parameters :
//     in  : 
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void GetUnrebasedVirtualAddress(CListview* pListview)
{
    int iItem;
    int istscanfRes;
    TCHAR psz[MAX_PATH];

    TCHAR pszModuleShortName[MAX_PATH];
    TCHAR pszModuleFullName[MAX_PATH];
    ULONGLONG RelativeAddress=0;

    if (IsBadReadPtr(pListview,sizeof(CListview*)))
        return;

    // get first selected item
    iItem=pListview->GetSelectedIndex();
    if (iItem<0)
    {
        // if not found
        MessageBox(mhWndDialog,_T("No item selected"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // retrieve text of ColumnsIndexCallerRelativeIndex
    pListview->GetItemText(iItem,CApiOverride::ColumnsIndexCallerRelativeIndex,psz,MAX_PATH);

    if (*psz==0)
    {
        // if not found
        MessageBox(mhWndDialog,_T("Error parsing Caller Relative Address"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // parse text
    istscanfRes=_stscanf(psz,_T("%s + 0x%p"),pszModuleShortName,&RelativeAddress);
    if (istscanfRes!=2)
    {
        MessageBox(mhWndDialog,_T("Error parsing Caller Relative Address"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // retrieve module full text
    pListview->GetItemText(iItem,CApiOverride::ColumnsIndexCallerFullPath,pszModuleFullName,MAX_PATH);
    if (*pszModuleFullName==0)
    {
        // if not found
        MessageBox(mhWndDialog,_T("Error retreiving module path"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // find image base in PE header of pszModuleFullName
    CPE PE(pszModuleFullName);
    RelativeAddress=PE.GetUnrebasedVirtualAddress(RelativeAddress);

    // on error 
    if (RelativeAddress==(ULONGLONG)-1)
    {
        // if not found
        MessageBox(mhWndDialog,_T("Error retreiving relative address"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // display result
    _stprintf(psz,_T("The non rebased virtual address is 0x%p"),RelativeAddress);
    MessageBox(mhWndDialog,psz,_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);

}
//-----------------------------------------------------------------------------
// Name: FindNextFailure
// Object: find next call failure
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void FindNextFailure()
{
    LOG_LIST_ENTRY* pLogEntry=NULL;
    int StartItemIndex=pListview->GetSelectedIndex()+1;

    // if we are at the end of the list
    if (StartItemIndex>=pListview->GetItemCount())
    {
        // we can't find more item
        MessageBox(mhWndDialog,_T("No Failure Found"),_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
        return;
    }
    
    // StartItemIndex check
    if (StartItemIndex<0)
        StartItemIndex=0;


    // set single selection style
    LONG_PTR Styles=GetWindowLongPtr(pListview->GetControlHandle(),GWL_STYLE);
    SetWindowLongPtr(pListview->GetControlHandle(),GWL_STYLE,Styles|LVS_SINGLESEL);

    // search for next matching item in listview
    for(int cnt=StartItemIndex;cnt<pListview->GetItemCount();cnt++)
    {
        if(!pListview->GetItemUserData(cnt,(LPVOID*)(&pLogEntry)))
            continue;
        if (pLogEntry==0)
            continue;
        if (IsBadReadPtr(pLogEntry,sizeof(LOG_LIST_ENTRY)))
            continue;
        if (pLogEntry->Type!=ENTRY_LOG)
            continue;
        if (IsBadReadPtr(pLogEntry->pLog,sizeof(LOG_ENTRY)))
            continue;

        // stop if item matchs
        if (pLogEntry->pLog->pHookInfos->bFailure)
        {
            // select item
            pListview->SetSelectedState(cnt,TRUE,FALSE);

            // assume item is visible
            pListview->ScrollTo(cnt);

            // restore style
            SetWindowLongPtr(pListview->GetControlHandle(),GWL_STYLE,Styles);

            return;
        }
    }
    // restore style
    SetWindowLongPtr(pListview->GetControlHandle(),GWL_STYLE,Styles);
    // if no item found
    MessageBox(mhWndDialog,_T("No Failure Found"),_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
}
//-----------------------------------------------------------------------------
// Name: FindPreviousFailure
// Object: find previous call failure
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void FindPreviousFailure()
{
    LOG_LIST_ENTRY* pLogEntry=NULL;
    int StartItemIndex=pListview->GetSelectedIndex()-1;

    // if we are at the begin of the list
    if (StartItemIndex<0)
    {
        // we can't find more item
        MessageBox(mhWndDialog,_T("No Failure Found"),_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
        return;
    }
    
    // StartItemIndex check
    if (StartItemIndex>pListview->GetItemCount()-1)
        StartItemIndex=pListview->GetItemCount()-1;


    // set single selection style
    LONG_PTR Styles=GetWindowLongPtr(pListview->GetControlHandle(),GWL_STYLE);
    SetWindowLongPtr(pListview->GetControlHandle(),GWL_STYLE,Styles|LVS_SINGLESEL);

    // search for previous matching item in listview
    for(int cnt=StartItemIndex;cnt>=0;cnt--)
    {
        if(!pListview->GetItemUserData(cnt,(LPVOID*)(&pLogEntry)))
            continue;
        if (pLogEntry==0)
            continue;
        if (IsBadReadPtr(pLogEntry,sizeof(LOG_LIST_ENTRY)))
            continue;
        if (pLogEntry->Type!=ENTRY_LOG)
            continue;
        if (IsBadReadPtr(pLogEntry->pLog,sizeof(LOG_ENTRY)))
            continue;

        // select item
        if (pLogEntry->pLog->pHookInfos->bFailure)
        {
            // Set Focus to Main dialog
            pListview->SetSelectedState(cnt,TRUE,FALSE);

            // assume item is visible
            pListview->ScrollTo(cnt);

            // restore style
            SetWindowLongPtr(pListview->GetControlHandle(),GWL_STYLE,Styles);

            return;
        }
    }

    // restore style
    SetWindowLongPtr(pListview->GetControlHandle(),GWL_STYLE,Styles);

    // if no item found
    MessageBox(mhWndDialog,_T("No Failure Found"),_T("Information"),MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
}
//-----------------------------------------------------------------------------
// Name: ListviewDetailsIndexToParamIndex
// Object: get parameter index from details listview row index
// Parameters :
//     in  : int MenuID : row index
//     out : 
//     return : index or -1 on failure
//-----------------------------------------------------------------------------
int ListviewDetailsIndexToParamIndex(int ListviewItemIndex)
{
    int ParamIndex;
    TCHAR psz[MAX_PATH];

    // check bounds
    ParamIndex = ListviewItemIndex-DetailListViewMinArg;
    if ((ParamIndex<0)||(ParamIndex>=(DetailListViewMaxArg-DetailListViewMinArg)))
        return -1;

    ParamIndex=0;
    // adjust Index according to number of lines taken by each param
    // count number of ParamXX in first column to know arg position in array
    for (int Cnt=DetailListViewMinArg;Cnt<=ListviewItemIndex;Cnt++)
    {
        pListviewDetails->GetItemText(Cnt,psz,MAX_PATH);
        if (_tcsnicmp(psz,_T("Param"),5)==0)
            ParamIndex++;
    }
    ParamIndex--;
    return ParamIndex;
}

#define LDSIC_RowName_Binary _T("Binary")
#define LDSIC_RowName_Char _T("char")
#define LDSIC_RowName_UChar _T("uchar")
#define LDSIC_RowName_Character _T("Character")

#define LDSIC_RowName_Short _T("short")
#define LDSIC_RowName_UShort _T("ushort")
#define LDSIC_RowName_WChar _T("Unicode")

#define LDSIC_RowName_Int _T("int")
#define LDSIC_RowName_UInt _T("uint")

#define LDSIC_RowName_Int64 _T("int64")
#define LDSIC_RowName_UInt64 _T("uint64")
#define LDSIC_ColumnValueIndex 1
TCHAR LDSIC_RowValue[MAX_PATH];
TCHAR* LDSIC_RowContent[2]={LDSIC_RowName_Binary,LDSIC_RowValue};
#define LDSIC_RowName (LDSIC_RowContent[0])

void ListviewDetailsSelectItemCallback_PrintByte(BYTE Value)
{
    int iValue = (int) Value;
    unsigned int uiValue = (unsigned int) Value;

    // binary
    LDSIC_RowName = LDSIC_RowName_Binary;
    CStringConverter::ValueToBinaryString<BYTE>(Value,LDSIC_RowValue);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // char
    LDSIC_RowName = LDSIC_RowName_Char;
    _stprintf(LDSIC_RowValue,_T("%d"),iValue);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // uchar
    LDSIC_RowName = LDSIC_RowName_UChar;
    _stprintf(LDSIC_RowValue,_T("%u"),uiValue);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // character
    LDSIC_RowName = LDSIC_RowName_Character;
    _stprintf(LDSIC_RowValue,_T("%c"),Value);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);
}

void ListviewDetailsSelectItemCallback_PrintWORD(WORD Value)
{
    int iValue = (int) Value;
    unsigned int uiValue = (unsigned int) Value;

    // binary
    LDSIC_RowName = LDSIC_RowName_Binary;
    CStringConverter::ValueToBinaryString<USHORT>(Value,LDSIC_RowValue);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // short
    LDSIC_RowName = LDSIC_RowName_Short;
    _stprintf(LDSIC_RowValue,_T("%d"),iValue);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // ushort
    LDSIC_RowName = LDSIC_RowName_UShort;
    _stprintf(LDSIC_RowValue,_T("%u"),uiValue);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // wild character
    LDSIC_RowName = LDSIC_RowName_WChar;
    _stprintf(LDSIC_RowValue,_T("%wc"),Value);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);
}

void ListviewDetailsSelectItemCallback_PrintDWORD(DWORD Value)
{
    // binary
    LDSIC_RowName = LDSIC_RowName_Binary;
    CStringConverter::ValueToBinaryString<DWORD>(Value,LDSIC_RowValue);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // short
    LDSIC_RowName = LDSIC_RowName_Int;
    _stprintf(LDSIC_RowValue,_T("%d"),Value);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // ushort
    LDSIC_RowName = LDSIC_RowName_UInt;
    _stprintf(LDSIC_RowValue,_T("%u"),Value);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);
}

void ListviewDetailsSelectItemCallback_PrintQWORD(UINT64 Value)
{
    // binary
    LDSIC_RowName = LDSIC_RowName_Binary;
    CStringConverter::ValueToBinaryString<UINT64>(Value,LDSIC_RowValue);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // short
    LDSIC_RowName = LDSIC_RowName_Int64;
    _stprintf(LDSIC_RowValue,_T("%I64d"),Value);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);

    // ushort
    LDSIC_RowName = LDSIC_RowName_UInt64;
    _stprintf(LDSIC_RowValue,_T("%I64u"),Value);
    pListviewDetailsTypesDisplay->AddItemAndSubItems(2,LDSIC_RowContent,(BOOL)FALSE,(LPVOID)NULL);
}

//-----------------------------------------------------------------------------
// Name: ListviewDetailsSelectItemCallback
// Object: static Call back for detail list view item selection
// Parameters :
//     in  : int MenuID : Id of menu clicked
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ListviewDetailsSelectItemCallback(int ItemIndex,int SubItemIndex,LPVOID UserParam)
{
    UNREFERENCED_PARAMETER(SubItemIndex);
    UNREFERENCED_PARAMETER(UserParam);

    if (ItemIndex<0)
        return;
    if (IsBadReadPtr(pLogListviewDetails,sizeof(LOG_ENTRY)))
        return;

    pListviewDetailsTypesDisplay->Clear();
    int ParamIndex = ListviewDetailsIndexToParamIndex(ItemIndex);
    if (ParamIndex<0)
    {
        // return value
        if ( ItemIndex == DetailListViewReturnValueIndex ) 
        {
#ifdef _WIN64
            ListviewDetailsSelectItemCallback_PrintQWORD(pLogListviewDetails->pHookInfos->ReturnValue);
#else
            ListviewDetailsSelectItemCallback_PrintDWORD((DWORD)pLogListviewDetails->pHookInfos->ReturnValue);
#endif
        }
        // last error
        else if ( ItemIndex == DetailListViewLastErrorIndex ) 
        {
#ifdef _WIN64
            TO CHECK
            ListviewDetailsSelectItemCallback_PrintQWORD(pLogListviewDetails->pHookInfos->dwLastError);
#else
            ListviewDetailsSelectItemCallback_PrintDWORD(pLogListviewDetails->pHookInfos->dwLastError);
#endif
        }
        return;
    }

    PARAMETER_LOG_INFOS* pLog=&pLogListviewDetails->ParametersInfoArray[ParamIndex];
    DWORD dwType = pLog->dwType & SIMPLE_TYPE_FLAG_MASK ;
    switch(dwType)// basic types
    {
    case PARAM_PSTR:
    case PARAM_PWSTR:
    case PARAM_PUNICODE_STRING:
    case PARAM_PANSI_STRING:
        break;
    case PARAM_UCHAR:
    case PARAM_BOOLEAN:
    case PARAM_BYTE:
    case PARAM_CHAR:
        ListviewDetailsSelectItemCallback_PrintByte((BYTE)pLog->Value);
        break;

    case PARAM_WORD:
    case PARAM_USHORT:
    case PARAM_WPARAM:
    case PARAM_WCHAR:
    case PARAM_SHORT:
        ListviewDetailsSelectItemCallback_PrintWORD((WORD)pLog->Value);
        break;
    case PARAM_ULONG:
    case PARAM_UINT:
    case PARAM_DWORD:
    case PARAM_REGSAM:
    case PARAM_ACCESS_MASK:
    case PARAM_LCID:
    case PARAM_COLORREF:
    case PARAM_SECURITY_INFORMATION:
        // 4 byte signed value param
    case PARAM_INT:
    case PARAM_BOOL:
    case PARAM_LONG:
    case PARAM_NTSTATUS:
#ifdef _WIN64
        TODO
#else
    case PARAM_HANDLE:
    case PARAM_HINSTANCE:
    case PARAM_HWND:
    case PARAM_HMODULE:
    case PARAM_HDC:
    case PARAM_HMENU:
    case PARAM_HIMAGELIST:
    case PARAM_HDESK:
    case PARAM_HBRUSH:
    case PARAM_HRGN:
    case PARAM_HDPA:
    case PARAM_HDSA:
    case PARAM_WNDPROC:
    case PARAM_DLGPROC:
    case PARAM_FARPROC:
    case PARAM_SIZE_T:
    case PARAM_HKEY:
    case PARAM_HPALETTE:
    case PARAM_HFONT:
    case PARAM_HMETAFILE:
    case PARAM_HGDIOBJ:
    case PARAM_HCOLORSPACE:
    case PARAM_HBITMAP:
    case PARAM_HICON:
    case PARAM_HCONV:
    case PARAM_HSZ:
    case PARAM_HDDEDATA:
    case PARAM_SC_HANDLE:
    case PARAM_HCERTSTORE:
    case PARAM_HGLOBAL:
    case PARAM_ULONG_PTR: 
    case PARAM_HCRYPTPROV:
    case PARAM_HCRYPTKEY: 
    case PARAM_HCRYPTHASH:
    case PARAM_SOCKET:    
    case PARAM_PSID:
    case PARAM_PFNCALLBACK:
    case PARAM_PSECURITY_DESCRIPTOR:
    case PARAM_LSA_HANDLE:
    case PARAM_POINTER:
    case PARAM_VOID:
    case PARAM_LONG_PTR:
    case PARAM_LPARAM:
#endif
        ListviewDetailsSelectItemCallback_PrintDWORD((DWORD)pLog->Value);
        break;

        break;
    case PARAM_INT64:
        {
#ifdef _WIN64
            TODO
#else
            if (pLog->pbValue)
            {
                ListviewDetailsSelectItemCallback_PrintQWORD(*(UINT64*)pLog->pbValue);
            }
#endif
        }
        break;
    // for pointer, we can't do the same as it may be an array
    // for struct and other unknown type, we just use 
    default:// complex type, expanded struct / array of type
        {
            
            TCHAR StrValue[MAX_PATH];
            TCHAR StrItemName[64];
            TCHAR* StrValueBegin;
            TCHAR* OldStrValueBegin;
            UINT64 Value; 
            SIZE_T KnownDataSize;
            SIZE_T ComputDataSize;

            switch(dwType)
            {
            case PARAM_PINT64:
            case PARAM_PTRACEHANDLE:
                KnownDataSize = 8;
                break;
            case PARAM_PBOOL:
            case PARAM_PINT:
            case PARAM_PLONG:
            case PARAM_PACCESS_MASK:
            case PARAM_PUINT:
            case PARAM_PULONG:
            case PARAM_PDWORD:
            case PARAM_PCOLORREF:
                KnownDataSize = 4;
                break;

            case PARAM_PHANDLE:
            case PARAM_PHKEY:
            case PARAM_PHMODULE:
            case PARAM_PSIZE_T:
            case PARAM_PSOCKET:
            case PARAM_PPSID:
            case PARAM_PPSECURITY_DESCRIPTOR:
            case PARAM_PLONG_PTR:
            case PARAM_PULONG_PTR:
            case PARAM_PLSA_HANDLE:
            case PARAM_PHICON:
            case PARAM_PPOINTER:
            case PARAM_PPVOID:
            #ifdef _WIN64
                TODO
            #else
                KnownDataSize = 4;
            #endif
                break;
            case PARAM_PWORD:
            case PARAM_PSHORT:
                KnownDataSize = 2;
                break;
            case PARAM_PBOOLEAN:
            case PARAM_PUCHAR:
            case PARAM_PBYTE:
                KnownDataSize = 1;
                break;
            default :
                KnownDataSize = 0;
                break;
            }

            // get data text
            pListviewDetails->GetItemText(ItemIndex,LDSIC_ColumnValueIndex,StrValue,MAX_PATH);

            OldStrValueBegin = StrValue;

            for(SIZE_T RowItemCnt=0;;RowItemCnt++,OldStrValueBegin=StrValueBegin+2) // in case there's more than one item in a single line (like array of point)
            {
                StrValueBegin = _tcsstr(OldStrValueBegin,_T("0x"));
                if (!StrValueBegin)// no value
                    break;
                // take the max possible value
                CStringConverter::StringToUnsignedInt64(StrValueBegin,&Value);

                if (RowItemCnt) // not for first loop
                {
                    // add an empty line row an "Item X" row
                    pListviewDetailsTypesDisplay->AddItem(_T(""),0,FALSE);
                    _stprintf(StrItemName,_T("Item %u"),RowItemCnt+1);
                    pListviewDetailsTypesDisplay->AddItem(StrItemName,0,FALSE);
                }

                if (Value & 0xFFFFFFFF00000000)
                    ComputDataSize = 8;
                else if (Value & 0xFFFF0000)
                    ComputDataSize = 4;
                else if (Value & 0xFF00)
                    ComputDataSize = 2;
                else // if (Value & 0xFF)
                    ComputDataSize = 1;

                ComputDataSize = __max(ComputDataSize,KnownDataSize);

                switch (ComputDataSize)
                {
                case 8:
                    ListviewDetailsSelectItemCallback_PrintQWORD(Value);
                    break;
                case 4:
                    ListviewDetailsSelectItemCallback_PrintDWORD((DWORD)Value);
                    break;
                case 2:
                    ListviewDetailsSelectItemCallback_PrintWORD((WORD)Value);
                    break;
                case 1:
                    ListviewDetailsSelectItemCallback_PrintByte((BYTE)Value);
                    break;
                }
            }
        }
        break;
    }

}

//-----------------------------------------------------------------------------
// Name: ListviewDetailsPopUpMenuItemClickCallback
// Object: static Call back for list view popup menu item click
// Parameters :
//     in  : int MenuID : Id of menu clicked
//     out : 
//     return : 
//-----------------------------------------------------------------------------
void ListviewDetailsPopUpMenuItemClickCallback(UINT MenuID,LPVOID UserParam)
{
    UNREFERENCED_PARAMETER(UserParam);
    if (IsBadReadPtr(pLogListviewDetails,sizeof(LOG_ENTRY)))
    {
        MessageBox(mhWndDialog,_T("This action can't be done :\r\nLog has already been removed from memory"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
        return;
    }

    // get first selected item
    int iItem=pListviewDetails->GetSelectedIndex();
    if (iItem<0)
        return;

    // find clicked menu item (check menu item name)
    if (MenuID==ListviewDetailsMenuIDHexDisplay)
    {
        // find parameter matching the selected item
        int ParamIndex=iItem-DetailListViewMinArg;
        // check index
        // don't use pLogListviewDetails->pHookInfos->bNumberOfParameters as a single param can be represented on multiple lines
        if ((ParamIndex<0)||(ParamIndex>=(DetailListViewMaxArg-DetailListViewMinArg)))
        {
            // check for call stack parameters
            int StackIndex=iItem-DetailListViewMinStack;

            
            if (StackIndex==0)
            {
                // stack == returned item
                return;
            }
            else
            {
                // convert StackIndex to index of pLogListviewDetails->CallSackInfoArray
                StackIndex--;
                // check bounds
                if ((StackIndex<0)||(StackIndex>=pLogListviewDetails->pHookInfos->CallStackSize))
                    return;

                CHexDisplay::Show(mhInstance,mhWndDialog,pLogListviewDetails->CallSackInfoArray[StackIndex].Parameters,pLogListviewDetails->pHookInfos->CallStackEbpRetrievalSize);
            }
        }
        else // parameters of the current call
        {

            PBYTE Address;
            DWORD Size;

            ParamIndex=ListviewDetailsIndexToParamIndex(iItem);
            if (ParamIndex<0)
                return;

            // if pointed data
            if (pLogListviewDetails->ParametersInfoArray[ParamIndex].dwSizeOfPointedValue>0)
            {
                Address=pLogListviewDetails->ParametersInfoArray[ParamIndex].pbValue;
                Size=pLogListviewDetails->ParametersInfoArray[ParamIndex].dwSizeOfPointedValue;
            }
            // if more than 4 bytes param
            else if (pLogListviewDetails->ParametersInfoArray[ParamIndex].dwSizeOfData>sizeof(DWORD))
            {
                Address=pLogListviewDetails->ParametersInfoArray[ParamIndex].pbValue;
                Size=pLogListviewDetails->ParametersInfoArray[ParamIndex].dwSizeOfData;
            }
            else
            {
                Address=(PBYTE)(&pLogListviewDetails->ParametersInfoArray[ParamIndex].Value);
                Size=sizeof(DWORD);
            }

            // special case of parsed types, adjust pointer to data
            // even with this hack, data will be represented as they are stored internally 
            // in Winapioverride (see XXX_PARSED data encoding and decoding in SupportedParameters.cpp)
            switch(pLogListviewDetails->ParametersInfoArray[ParamIndex].dwType)
            {
            case PARAM_SAFEARRAY_PARSED:
            case PARAM_VARIANT_PARSED:
                Address+=sizeof(DWORD);
                Size-=sizeof(DWORD);
                break;
            }

            CHexDisplay::Show(mhInstance,mhWndDialog,Address,Size);
        }

    }
}
void SetGuiMode(BOOL bStateStarted)
{
    EnableMonitoringFakingInterface(bStateStarted);
}
//-----------------------------------------------------------------------------
// Name: EnableMonitoringFakingInterface
// Object: Enable / disable monitoring and faking user interface
// Parameters :
//     in  : BOOL bEnable : TRUE to enable, FALSE to disable
//     out :
//     return : 
//-----------------------------------------------------------------------------
void EnableMonitoringFakingInterface(BOOL bEnable)
{
    if(!pSplitterConfig)
        return;

    BOOL ModuleFiltersVisible;
    // update global flag state
    bUserInterfaceInStartedMode=bEnable;

    CDialogHelper::ShowGroup(GetDlgItem(mhWndDialog,IDC_STATIC_MONITORING),bEnable);
    CDialogHelper::ShowGroup(GetDlgItem(mhWndDialog,IDC_STATIC_FAKING),bEnable);

    if (bEnable)
    {
        pSplitterConfig->Hide();
        pSplitterLoadMonitoringAndFaking->Show();
    }
    else
    {
        pSplitterLoadMonitoringAndFaking->Hide();
        pSplitterConfig->Show();
    }

    if (bEnable)
    {
        // make modules filters non visible in running state by default to avoid a flashing effect if they're hidden
        CDialogHelper::ShowGroup(GetDlgItem(mhWndDialog,IDC_STATIC_MODULES_FILTERS),!bEnable);
    }
    else // in stopped config, make modules filter visible according to pSplitterConfig->IsCollapsed
    {
        ModuleFiltersVisible=!pSplitterConfig->IsCollapsed();
        CDialogHelper::ShowGroup(GetDlgItem(mhWndDialog,IDC_STATIC_MODULES_FILTERS),ModuleFiltersVisible);
    }

    pToolbar->EnableButton(IDC_BUTTON_LOAD_LOGS,!bEnable);
    pToolbar->EnableButton(IDC_BUTTON_ANALYSIS,!bEnable);

    pToolbar->EnableButton(IDC_BUTTON_START_STOP_MONITORING,bEnable);
    pToolbar->EnableButton(IDC_BUTTON_START_STOP_FAKING,bEnable);
    pToolbar->EnableButton(IDC_BUTTON_DUMP,bEnable);
    pToolbar->EnableButton(IDC_BUTTON_INTERNAL_CALL,bEnable);
    pToolbar->EnableButton(IDC_BUTTON_MODULES_FILTERS,bEnable && (!pOptions->bOnlyBaseModule));
    if (bEnable)
    {
        // remove analysis checked state
        pToolbar->SetButtonState(IDC_BUTTON_ANALYSIS,pToolbar->GetButtonState(IDC_BUTTON_ANALYSIS)&(~TBSTATE_CHECKED));
    }
    EnableWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_MODULES_FILTERS),bEnable && (!pOptions->bOnlyBaseModule));
    // enable idMenuComToolsHookedComObjectInteraction only if started and COM hooking is queried
    pCOMToolsPopUpMenu->SetEnabledState(idMenuComToolsHookedComObjectInteraction,
                                        bEnable && (pToolbar->GetButtonState(IDC_BUTTON_COM)&(TBSTATE_CHECKED)));

    ///////////////////////////////////////////
    // upper interface part enabling/disabling
    ///////////////////////////////////////////
    if (bEnable
        || (!pSplitterConfig->IsCollapsed()))
    {
        // we do the inverse of lower part
        bEnable=!bEnable;
        //// lock unlock radios (no use until they are hidden)
        //EnableWindow(GetDlgItem(mhWndDialog,IDC_RADIO_ALL_PROCESS),bEnable);
        //EnableWindow(GetDlgItem(mhWndDialog,IDC_RADIO_APP_NAME),bEnable);
        //EnableWindow(GetDlgItem(mhWndDialog,IDC_RADIO_PID),bEnable);
        //EnableWindow(GetDlgItem(mhWndDialog,IDC_CHECK_5_BYTES_HOOK),bEnable);
        /*
        ShowWindow(GetDlgItem(mhWndDialog,IDC_RADIO_ALL_PROCESS),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_RADIO_APP_NAME),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_RADIO_PID),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_STATICPIDSELECT),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_STATIC_MS),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_STATIC_MS2),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_STATIC_PICTURE_SELECT_TARGET),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_STATIC_PROCESS_ID),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_STATIC_COMMAND_LINE),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_STATIC_APP_PATH),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_CHECK_START_APP_WAIT_TIME),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_CHECK_INJECT_IN_ALL_ONLY_AFTER),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_PROCESS_FILTERS),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_BUTTON_BROWSE_APP_PATH),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_EDIT_APP_PATH),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_EDIT_CMD_LINE),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_EDIT_PID),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_EDIT_INSERT_AFTER),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_CHECK_START_APP_EXE_IAT_HOOKING),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_EDIT_RESUMETIME),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_CHECK_STOP_AND_KILL_APP),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_EDIT_STOP_AND_KILL_APP_TIME),bEnable);
        ShowWindow(GetDlgItem(mhWndDialog,IDC_STATIC_MS3),bEnable);
        */
        CDialogHelper::ShowGroup(::GetDlgItem(mhWndDialog,IDC_STATICPIDSELECT),bEnable);
    }

    // auto size toolbar
    pToolbar->Autosize();

    Resize(bUserInterfaceInStartedMode);

    if (!bUserInterfaceInStartedMode)
    {
        // scroll to last item
        pListview->ScrollTo(pListview->GetItemCount()-1);

        //////////////////////////////////////////////
        //reset state of start/stop monitoring faking
        //////////////////////////////////////////////

        // put IDC_BUTTON_START_STOP_MONITORING caption to Stop
        pToolbar->ReplaceToolTipText(IDC_BUTTON_START_STOP_MONITORING,_T("Pause Monitoring"));
        pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_MONITORING,CToolbar::ImageListTypeEnable,IDI_ICON_PAUSE_MONITORING);
        pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_MONITORING,CToolbar::ImageListTypeHot,IDI_ICON_PAUSE_MONITORING_HOT);


        // put IDC_BUTTON_START_STOP_FAKING caption to Stop
        pToolbar->ReplaceToolTipText(IDC_BUTTON_START_STOP_FAKING,_T("Pause Overriding"));
        pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_FAKING,CToolbar::ImageListTypeEnable,IDI_ICON_PAUSE_FAKING);
        pToolbar->ReplaceIcon(IDC_BUTTON_START_STOP_FAKING,CToolbar::ImageListTypeHot,IDI_ICON_PAUSE_FAKING_HOT);
    }
}

void CallBackSplitterConfigCollapsedStateChange(BOOL NewCollapsedState,PVOID UserParam)
{
    UNREFERENCED_PARAMETER(UserParam);
    BOOL bEnable=!NewCollapsedState;

    CDialogHelper::ShowGroup(GetDlgItem(mhWndDialog,IDC_STATICPIDSELECT),bEnable);
    CDialogHelper::ShowGroup(GetDlgItem(mhWndDialog,IDC_STATIC_MODULES_FILTERS),bEnable);

    // let the resize func do all the job
    Resize(FALSE);
}

void SupportedParametersReportError(IN TCHAR* const ErrorMessage,IN PBYTE const UserParam)
{
    UNREFERENCED_PARAMETER(UserParam);
    MessageBox(mhWndDialog,ErrorMessage,_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
}

//-----------------------------------------------------------------------------
// Name: BrowseApplicationPath
// Object: open browse dialog for selecting application to start
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void BrowseApplicationPath()
{
    TCHAR pszFile[MAX_PATH]=_T("");

    // open dialog
    OPENFILENAME ofn;
    memset(&ofn,0,sizeof (OPENFILENAME));
    ofn.lStructSize=sizeof (OPENFILENAME);
    ofn.hwndOwner=mhWndDialog;
    ofn.hInstance=mhInstance;
    ofn.lpstrFilter=_T("exe\0*.exe\0All\0*.*\0");
    ofn.nFilterIndex = 1;
    ofn.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt=_T("exe");
    ofn.lpstrFile=pszFile;
    ofn.nMaxFile=MAX_PATH;
    
    if (!GetOpenFileName(&ofn))
        return;

    SetDlgItemText(mhWndDialog,IDC_EDIT_APP_PATH,pszFile);
}

//-----------------------------------------------------------------------------
// Name: BrowseMonitoringFile
// Object: open browse dialog for loading monitoring files
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void BrowseMonitoringFile()
{
    TCHAR pszFile[MAX_PATH]=_T("");

    // open dialog
    OPENFILENAME ofn;
    memset(&ofn,0,sizeof (OPENFILENAME));
    ofn.lStructSize=sizeof (OPENFILENAME);
    ofn.hwndOwner=mhWndDialog;
    ofn.hInstance=mhInstance;
    ofn.lpstrFilter=_T("txt\0*.txt\0");
    ofn.nFilterIndex = 1;
    ofn.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt=_T("txt");
    ofn.lpstrFile=pszFile;
    ofn.nMaxFile=MAX_PATH;
    
    if (!GetOpenFileName(&ofn))
        return;

    SetDlgItemText(mhWndDialog,IDC_EDIT_MONITORING_FILE,pszFile);
}

//-----------------------------------------------------------------------------
// Name: BrowseFakingFile
// Object: open browse dialog for loading faking dll
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void BrowseFakingFile()
{
    TCHAR pszFile[MAX_PATH]=_T("");

    // open dialog
    OPENFILENAME ofn;
    memset(&ofn,0,sizeof (OPENFILENAME));
    ofn.lStructSize=sizeof (OPENFILENAME);
    ofn.hwndOwner=mhWndDialog;
    ofn.hInstance=mhInstance;
    ofn.lpstrFilter=_T("dll\0*.dll\0");
    ofn.nFilterIndex = 1;
    ofn.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt=_T("dll");
    ofn.lpstrFile=pszFile;
    ofn.nMaxFile=MAX_PATH;
    
    if (!GetOpenFileName(&ofn))
        return;

    SetDlgItemText(mhWndDialog,IDC_EDIT_FAKING_FILE,pszFile);
}

void BrowseNotLoggedModuleList()
{
    TCHAR pszFile[MAX_PATH]=_T("");

    // open dialog
    OPENFILENAME ofn;
    memset(&ofn,0,sizeof (OPENFILENAME));
    ofn.lStructSize=sizeof (OPENFILENAME);
    ofn.hwndOwner=mhWndDialog;
    ofn.hInstance=mhInstance;
    ofn.lpstrFilter=_T("txt\0*.txt\0");
    ofn.nFilterIndex = 1;
    ofn.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt=_T("txt");
    ofn.lpstrFile=pszFile;
    ofn.nMaxFile=MAX_PATH;
    
    if (!GetOpenFileName(&ofn))
        return;

    SetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pszFile);
}

//-----------------------------------------------------------------------------
// Name: ProcessCustomDrawListViewDetails
// Object: Handle NM_CUSTOMDRAW message (set listview details colors)
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
LRESULT ProcessCustomDrawListViewDetails (LPARAM lParam)
{
    LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;
    switch(lplvcd->nmcd.dwDrawStage) 
    {
        case CDDS_PREPAINT : //Before the paint cycle begins
            //request notifications for individual listview items
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: //Before an item is drawn
            // lplvcd->nmcd.dwItemSpec // item index
            // to request notification for subitems
            return CDRF_NOTIFYSUBITEMDRAW;
        // for subitem customization
        case CDDS_SUBITEM | CDDS_ITEMPREPAINT: //Before a subitem is drawn
        // lplvcd->nmcd.dwItemSpec // item number
        // lplvcd->iSubItem // subitem number

        // customize subitem appearance for column 0
        if (lplvcd->iSubItem==0)
        {
            lplvcd->clrTextBk = ListViewColorMessageBackGround;
        }
        else
        {
            //customize item appearance
            if (lplvcd->nmcd.dwItemSpec%2==1)
                // set a background color
                lplvcd->clrTextBk = ListViewColorDefaultBackGround;
            else
                lplvcd->clrTextBk =RGB(255,255,255);
        }

        //else
        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

//-----------------------------------------------------------------------------
// Name: ProcessCustomDraw
// Object: Handle NM_CUSTOMDRAW message (set listview colors)
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
LRESULT ProcessCustomDraw (LPARAM lParam)
{
    LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;
    
    BOOL bRet;
    LOG_LIST_ENTRY* pLogEntry=NULL;

    switch(lplvcd->nmcd.dwDrawStage) 
    {
        case CDDS_PREPAINT : //Before the paint cycle begins
            //request notifications for individual listview items
            return CDRF_NOTIFYITEMDRAW;
			
        case CDDS_ITEMPREPAINT: //Before an item is drawn
            // lplvcd->nmcd.dwItemSpec // item index
            // to request notification for subitems
            // return CDRF_NOTIFYSUBITEMDRAW;
            if (!pListview)
                return CDRF_DODEFAULT;
            // pLogList->Lock(); locking here create deadlock --> we have to assume associated data is valid until item is present in listview
            bRet=pListview->GetItemUserData((int)lplvcd->nmcd.dwItemSpec,(LPVOID*)(&pLogEntry));
            if (bRet)
            {
                if (pLogEntry==0)
                    bRet=FALSE;
                else
                {
                    if (IsBadReadPtr(pLogEntry,sizeof(LOG_LIST_ENTRY)))
                        bRet=FALSE;
                }
            }
            if (bRet)
            {
                if (pLogEntry->Type==ENTRY_LOG)
                {
                    if (IsBadReadPtr(pLogEntry->pLog,sizeof(LOG_ENTRY)))
                    {
                        // pLogList->Unlock();
                        return CDRF_DODEFAULT;
                    }
                    if (pLogEntry->pLog->pHookInfos->bFailure)
                        // set a background color
                        lplvcd->clrTextBk = ListViewColorFailureBackGround;
                    else
                    {
                        //customize item appearance
                        if (lplvcd->nmcd.dwItemSpec%2==1)
                            // set a background color
                            lplvcd->clrTextBk = ListViewColorDefaultBackGround;
                    }
                    // pLogList->Unlock();
                    return CDRF_DODEFAULT;
                }
                else// item is a comment
                {
                    // set a background color
                    lplvcd->clrTextBk = ListViewColorMessageBackGround;

                    switch(pLogEntry->Type)
                    {
                    case ENTRY_MSG_WARNING:
                        // set a foreground color
                        lplvcd->clrText   = ListViewColorWarning;
                        break;
                    case ENTRY_MSG_ERROR:
                    case ENTRY_MSG_EXCEPTION:
                        // set a foreground color
                        lplvcd->clrText   = ListViewColorError;
                        break;
                    case ENTRY_MSG_INFORMATION:
                    default:
                        // set a foreground color
                        lplvcd->clrText   = ListViewColorInformation;
                        break;
                    }
                }
            }

            // pLogList->Unlock();

            //To set a custom font:
            //SelectObject(lplvcd->nmcd.hdc, <your custom HFONT>);
            // return CDRF_NEWFONT;

            return CDRF_DODEFAULT;
	
        // for subitem customization
        // case CDDS_SUBITEM | CDDS_ITEMPREPAINT: //Before a subitem is drawn
            // lplvcd->nmcd.dwItemSpec // item number
            // lplvcd->iSubItem // subitem number
            // // customize subitem appearance for column 0
            // lplvcd->clrText   = RGB(255,255,255);
            // lplvcd->clrTextBk = RGB(255,0,0);
            // //To set a custom font:
            // //SelectObject(lplvcd->nmcd.hdc, <your custom HFONT>);

            // return CDRF_NEWFONT;
    }
    return CDRF_DODEFAULT;
}

//-----------------------------------------------------------------------------
// Name: KeyboardProc
// Object: keyboard hook proc for shortcuts
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
LRESULT CALLBACK KeyboardProc (int code, WPARAM wParam, LPARAM lParam) 
{
    BOOL bMessageUsed=FALSE;
    LRESULT Res;
    if(code == HC_ACTION) 
    {
        BOOL bKeyDown=(BOOL)(lParam>>31);
        // BOOL bKeyUp=!bKeyDown;

        // we just check for control actions
        if (GetKeyState(VK_CONTROL) & 0x8000)
        {
            bMessageUsed=TRUE;
            switch(wParam)
            {
            case 0x42:// B --> String Hex
                if (bKeyDown)// catch only key down message
                    CConvertStringBytes::Show(mhInstance,mhWndDialog);
                break;

            case 0x44: // d --> Dump
                if (bKeyDown)// catch only key down message
                    if (bStarted)
                        Dump();
                break;
            case 0x45: // e --> Error Message
                if (bKeyDown)// catch only key down message
                    ShowLastErrorMessage(pListview);
                break;
            case 0x46: // f --> find 
                if (bKeyDown)// catch only key down message
                    CSearch::ShowDialog(mhInstance,mhWndDialog);
                break;
            case 0x49: // I --> Informations
                if (bKeyDown)// catch only key down message
                    About();
                break;
            case 0x47:// G --> Google
                if (bKeyDown)// catch only key down message
                    GoogleFuncName(pListview);
                break;
            case 0x4D:// M --> Memory
                if (bKeyDown)// catch only key down message
                    CMemoryUserInterface::Show(mhInstance,mhWndDialog);
                break;
            case 0x4E:// N
                if (bKeyDown)// catch only key down message
                    FindNextFailure();
                break;
            case 0x4F:// O --> open
                if (bKeyDown)// catch only key down message
                    LoadLogs();
                break;
            case 0x50:// P --> previous failure
                if (bKeyDown)// catch only key down message
                    FindPreviousFailure();
                break;
            case 0x51:// Q --> Compare
                if (bKeyDown)// catch only key down message
                    CompareLogs();
                break;
            case 0x52: // R -->Remote Call
                if (bKeyDown)// catch only key down message
                    if (bStarted)
                        CRemoteCall::ShowDialog(mhInstance,mhWndDialog);
                break;
            case 0x53:// S --> Save
                if (bKeyDown)// catch only key down message
                    SaveLogs();
                break;
            case 0x57:// W --> Online MSDN
                if (bKeyDown)// catch only key down message
                    ShowMSDNOnlineHelp(pListview);
                break;
            default:
                bMessageUsed=FALSE;
                break;
            }
        }
        else
        {
            switch(wParam)
            {
            case VK_F1:
            case VK_HELP:
                if (bKeyDown)// catch only key down message
                    Help();
                break;
            }
        }
    }

    // call next hook
    Res=CallNextHookEx(NULL, code, wParam, lParam);

    // if message has been taken into account return a non null value 
    // (avoid beep as for unsupported key)
    if (bMessageUsed)
        Res=1;

    return Res;
}
//-----------------------------------------------------------------------------
// Name: ListviewKeyDown
// Object: call back for listview only short cuts (called only if listview has focus)
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void ListviewKeyDown(WORD vKey)
{
    BOOL bCtrlKeyDown;
    // get control key state
    bCtrlKeyDown=GetKeyState(VK_CONTROL) & 0x8000;

    switch(vKey)
    {
    case VK_DELETE:
        RemoveSelectedLogs();
        break;
    case 0x43: // c --> Copy
        if (bCtrlKeyDown)
        {
            CopyFuncName(pListview);
        }
        break;
    }
}

//-----------------------------------------------------------------------------
// Name: SetWaitCursor
// Object: Set cursor to wait cursor or to normal cursor depending bSet
// Parameters :
//     in  : BOOL bSet : TRUE to set wait cursor, FALSE to restore normal cursor
//     out :
//     return : 
//-----------------------------------------------------------------------------
void SetWaitCursor(BOOL bSet)
{
    if (bSet)
    {
        WaitCursorCount++;
        SetCursor(hWaitCursor);
    }
    else
    {
        if (WaitCursorCount>0)
            WaitCursorCount--;
    }
    // force cursor update
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(pt.x,pt.y);
}

//-----------------------------------------------------------------------------
// Name: OnDropFile
// Object: called when user ends a drag and drop operation
// Parameters :
//     in  : HWND hWnd : window handle
//           HDROP hDrop : drop informations
//     out :
//     return : 
//-----------------------------------------------------------------------------
void OnDropFile(HWND hWnd, HDROP hDrop)
{
    TCHAR pszFileName[MAX_PATH];
    UINT NbFiles;
    UINT Cnt;
    POINT pt;
    HWND SubWindowHandle;

    // retrieve dialog subitem window handle
    DragQueryPoint(hDrop, &pt);
    ClientToScreen(hWnd,&pt);
    SubWindowHandle=WindowFromPoint(pt);

    // get number of files count in the drag and drop
    NbFiles=DragQueryFile(hDrop, 0xFFFFFFFF,NULL, MAX_PATH);
    // get first file name (in case we just need one name)
    DragQueryFile(hDrop, 0,pszFileName, MAX_PATH);

    // if application path
    if (SubWindowHandle==GetDlgItem(mhWndDialog,IDC_EDIT_APP_PATH))
    {
        // fill it
        SetDlgItemText(mhWndDialog,IDC_EDIT_APP_PATH,pszFileName);
    }
    // if module filter path
    else if (SubWindowHandle==GetDlgItem(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST))
    {
        // fill it
        SetDlgItemText(mhWndDialog,IDC_EDIT_FILTER_MODULE_LIST,pszFileName);
    }
    else if (SubWindowHandle==GetDlgItem(mhWndDialog,IDC_EDIT_FAKING_FILE))
    {
        // fill it
        SetDlgItemText(mhWndDialog,IDC_EDIT_FAKING_FILE,pszFileName);
    }
    else if (SubWindowHandle==GetDlgItem(mhWndDialog,IDC_EDIT_MONITORING_FILE))
    {
        // fill it
        SetDlgItemText(mhWndDialog,IDC_EDIT_MONITORING_FILE,pszFileName);
    }
    // ListViewMonitoringFiles handle
    else if(SubWindowHandle==GetDlgItem(mhWndDialog,IDC_LIST_MONITORING_FILES))
    {
        // load all files as monitoring files
        for (Cnt=0;Cnt<NbFiles;Cnt++)
        {
            DragQueryFile(hDrop, Cnt,pszFileName, MAX_PATH);
            LoadMonitoringFile(pszFileName);
        }
    }
    else if(SubWindowHandle==GetDlgItem(mhWndDialog,IDC_LIST_FAKING_FILES))
    {
        // load all files as overriding files
        for (Cnt=0;Cnt<NbFiles;Cnt++)
        {
            DragQueryFile(hDrop, Cnt,pszFileName, MAX_PATH);
            LoadOverridingFile(pszFileName);
        }
    }
    // if drop operation has not been already processed
    else
    {
        // default operation
        TCHAR* mpszFileName=_tcsdup(pszFileName);

        // load file as a log file if no loading operation already pending
        if (!hThreadLoadLogs)
            hThreadLoadLogs = ::CreateThread(0,0,LoadThread,mpszFileName,0,0);
    }

    DragFinish(hDrop);
}

//-----------------------------------------------------------------------------
// Name: ListViewSpecializedSavingFunction
// Object: allow us to save not displayed listview content, but our one
//         it is useful because listview representation of call may troncat some
//         parameters. So for ExportFullParametersContent option, we have to reparse
//         all parameters
// Parameters :
//     in  : HWND hWnd : window handle
//           HDROP hDrop : drop informations
//     out :
//     return : 
//-----------------------------------------------------------------------------
void ListViewSpecializedSavingFunction(HANDLE SavingHandle,int ItemIndex,int SubItemIndex,LPVOID UserParam)
{
    UNREFERENCED_PARAMETER(UserParam);
    BOOL SpecificSaving=FALSE;
    BOOL bRet;
    LOG_LIST_ENTRY* pLogEntry=NULL;
    if (SubItemIndex==CApiOverride::ColumnsIndexCall)
    {
        if (pExportPopUpMenu->IsChecked(idMenuExportFullParametersContent))
        {
            bRet=pListview->GetItemUserData(ItemIndex,(LPVOID*)(&pLogEntry));
            if (bRet)
            {
                if (pLogEntry==0)
                    bRet=FALSE;
                else
                {
                    if (IsBadReadPtr(pLogEntry,sizeof(LOG_LIST_ENTRY)))
                        bRet=FALSE;
                }
            }
            if (bRet)
            {
                // if log entry is a log not a report message
                if (pLogEntry->Type==ENTRY_LOG)
                {
                    if (!IsBadReadPtr(pLogEntry->pLog,sizeof(LOG_ENTRY)))
                        SpecificSaving=TRUE;
                }
            }
        }
    }
    if (SpecificSaving)
    {
        // pLogEntry->pLog has been filled
        TCHAR* ParamString;

        // save func name 
        pListview->SaveContentForSpecializedSavingFunction(SavingHandle,pLogEntry->pLog->pszApiName);

        // add '('
        pListview->SaveContentForSpecializedSavingFunction(SavingHandle,_T("("));

        // for each param
        for (BYTE cnt=0;cnt<pLogEntry->pLog->pHookInfos->bNumberOfParameters;cnt++)
        {
            // if not first item add ', '
            if (cnt)
                pListview->SaveContentForSpecializedSavingFunction(SavingHandle,_T(", "));

            // fully parse parameter
            CSupportedParameters::ParameterToString(pLogEntry->pLog->pszModuleName,&pLogEntry->pLog->ParametersInfoArray[cnt],&ParamString,TRUE);

            // write parameter content to file
            pListview->SaveContentForSpecializedSavingFunction(SavingHandle,ParamString);

            // free string param memory
            delete ParamString;
        }

        // add ')'
        pListview->SaveContentForSpecializedSavingFunction(SavingHandle,_T(")"));
    }
    else
    {
        DWORD TextLen=pListview->GetItemTextLen(ItemIndex,SubItemIndex);
        TCHAR* psz=(TCHAR*)_alloca(TextLen*sizeof(TCHAR));
        pListview->GetItemText(ItemIndex,SubItemIndex,psz,TextLen);
        pListview->SaveContentForSpecializedSavingFunction(SavingHandle,psz);
    }
}

//-----------------------------------------------------------------------------
// Name: CheckForUpdate
// Object: check for current software update
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
BOOL CheckForUpdate(BOOL bDisplayDialog,BOOL* pbNewVersionIsAvailable)
{
    return CSoftwareUpdate::CheckForUpdate(WinApioverride_PAD_Url,WinApioverrideVersionSignificantDigits,WinApioverrideDownloadLink,bDisplayDialog,pbNewVersionIsAvailable);
}
BOOL CheckForUpdate(BOOL* pbNewVersionIsAvailable,WCHAR** ppDownloadLink)
{
    // copy download link to output buffer
    SIZE_T Size = _tcslen(WinApioverrideDownloadLink)+1;
    *ppDownloadLink = new WCHAR[Size];
#if (defined(UNICODE)||defined(_UNICODE))
    _tcscpy(*ppDownloadLink,WinApioverrideDownloadLink);
#else
    CAnsiUnicodeConvert::AnsiToUnicode(WinApioverrideDownloadLink,*ppDownloadLink,Size);
#endif

    // check for update
    return CheckForUpdate(FALSE,pbNewVersionIsAvailable);
}
void CheckForUpdate()
{
    BOOL bNewVersionIsAvailable;
    CheckForUpdate(TRUE,&bNewVersionIsAvailable);
}
//-----------------------------------------------------------------------------
// Name: ReportBug
// Object: Send mail to author reporting bug
// Parameters :
//     in  : 
//     out :
//     return : 
//-----------------------------------------------------------------------------
void ReportBug()
{
    // see RFC 2368 for mailto protocol
    TCHAR* pszURL=_T("mailto:jacquelin.potier@free.fr?subject=WinApiOverride Bug Report&body=")
                  _T("Please provide the most possible details about the bug,%0D%0A")
                  _T("including the description of the problem itself.%0D%0A")
                  _T("Specify actions you have done (monitoring, overriding, ...),%0D%0A")
                  _T("and when it happens (at hooking start, hooking stop, ...)%0D%0A")
                  _T("Please include any error messages you received.%0D%0A")
                  _T("All these informations will be useful for a quicker bug correction%0D%0A%0D%0A")
                  _T("Thanks for report%0D%0A")
                  _T("Jacquelin")
                  ;
    if (((int)ShellExecute(NULL,_T("open"),pszURL,NULL,NULL,SW_SHOWNORMAL))<33)
    {
        MessageBox(mhWndDialog,_T("Error launching default mail client"),_T("Error"),MB_OK|MB_ICONERROR|MB_TOPMOST);
    }
}

void CrossThreadSetFocus(HWND hWnd)
{
    ::SendMessage(mhWndDialog,WM_USER_CROSS_THREAD_SET_FOCUS,0,(LPARAM)hWnd);
}