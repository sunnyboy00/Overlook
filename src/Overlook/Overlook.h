#ifndef _Overlook_Overlook_h_
#define _Overlook_Overlook_h_

#include "Common.h"
#include "QueryTable.h"
#include "DQN.h"

#include "SimBroker.h"
#include "AgentSystem.h"
#include "System.h"
#include "Core.h"
#include "CustomCtrl.h"
#include "ExposureTester.h"

#include "DataBridge.h"
#include "Indicators.h"
#include "QtStats.h"

#include "AgentCtrl.h"
#include "ManagerCtrl.h"

#include "GraphCtrl.h"
#include "GraphGroupCtrl.h"

#include "BrokerCtrl.h"

#define IMAGECLASS OverlookImg
#define IMAGEFILE <Overlook/Overlook.iml>
#include <Draw/iml_header.h>

namespace Overlook {


class Overlook : public TopWindow {
	
protected:
	friend class PrioritizerCtrl;
	
	// Main view
	TabCtrl					tabs;
	
	// Classic view
	ParentCtrl				visins;
	Splitter				droplist_split;
	DropList				ctrllist, symlist, tflist;
	Button					config;
	VectorMap<String, Ctrl*> ctrls;
	CustomCtrl*				prev_view;
	Core*					prev_core;
	TimeCallback			tc;
	
	// Exposure tester
	ExposureTester			exposurectrl;
	
	// Manager view
	ManagerCtrl				mgrctrl;
	
	
	RealtimeCtrl			rtctrl;
	StatusBar				status;
	
	Vector<String>			task_stack;
	
	void SetView();
	void Configure();
	void Data();
	
public:
	typedef Overlook CLASSNAME;
	Overlook();
	~Overlook();
	
	void Init();
	void Refresher();
	void PostRefresher() {tc.Kill(); PostCallback(THISBACK(Refresher));}
	void PostPushTask(String task) {PostCallback(THISBACK1(PushTask, task));}
	void PostPopTask() {PostCallback(THISBACK(PopTask));}
	void PushTask(String task);
	void PopTask();
	void RefreshTaskStatus();
};



}

#endif
	