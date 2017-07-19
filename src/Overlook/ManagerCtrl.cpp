#include "Overlook.h"

namespace Overlook {

GroupOverview::GroupOverview()
{
	CtrlLayout(*this);
	
	prog.Set(0, 1);
	sub.Set(0, 1);
	
	modelist.Add("Agent training");
	modelist.Add("Group optimizing");
	modelist.Add("Allow real-time trading");
	modelist.SetIndex(0);
	modelist <<= THISBACK(SetMode);
	
	epsilon <<= THISBACK(SetEpsilon);
}

void GroupOverview::SetMode() {
	int i = modelist.GetIndex();
	if (group)
		group->SetMode(i);
}

void GroupOverview::SetGroup(AgentGroup& group) {
	if (this->group) {
		this->group->WhenProgress.Clear();
		this->group->WhenSubProgress.Clear();
	}
	this->group = &group;
	this->group->WhenProgress = THISBACK(PostProgress);
	this->group->WhenSubProgress = THISBACK(PostSubProgress);
	
	enable.Set(group.enable_training);
	
	Data();
}

void GroupOverview::Progress(int actual, int total, String label) {
	prog.Set(actual, total);
	sub.Set(0, 1);
	this->label = label;
	lbl.SetLabel(Format("%s: %d/%d, %d/%d", label, actual, total, group->a1, group->t1));
}

void GroupOverview::SubProgress(int actual, int total) {
	sub.Set(actual, total);
	lbl.SetLabel(Format("%s: %d/%d, %d/%d", group->prog_desc, group->a0, group->t0, actual, total));
}

void GroupOverview::Data() {
	if (group) {
		lbl.SetLabel(Format("%s: %d/%d, %d/%d", group->prog_desc, group->a0, group->t0, group->a1, group->t1));
		prog.Set(group->a0, group->t0);
		sub.Set(group->a1, group->t1);
		
		String infostr;
		infostr << "Name: " << group->name << "\n";
		infostr << "Free-margin level: " << group->global_free_margin_level << "\n";
		infostr << "Reward period: " << group->agent_input_width << "x" << group->agent_input_height << "\n";
		infostr << "Reward period: " << group->group_input_width << "x" << group->group_input_height << "\n";
		infostr << "Signal freeze: " << (group->sig_freeze ? "True" : "False") << "\n";
		infostr << "Enable training: " << (group->enable_training ? "True" : "False") << "\n";
		infostr << "Created: " << Format("%", group->created) << "\n";
		infostr << "Snapshot data-size: " << group->data_size << "\n";
		for(int i = 0; i < group->tf_ids.GetCount(); i++) {
			infostr << "    tf" << i << ": " << group->tf_ids[i] << ", " << group->sys->GetPeriodString(group->tf_ids[i]) << "\n";
		}
		
		infostr << "\nDQN-agent parameters:\n" << group->param_str << "\n\n";
		
		if (!group->agents.IsEmpty())
			epsilon.SetData(group->agents[0].dqn.GetEpsilon());
		info.SetLabel(infostr);
	}
}









GroupTabCtrl::GroupTabCtrl() {
	group = NULL;
	
	Add(overview);
	Add(overview, "Overview");
	Add(trainingctrl);
	Add(trainingctrl, "Training");
	Add(snapctrl);
	Add(snapctrl, "Snapshot list");
	
	WhenSet << THISBACK(Data);
}

void GroupTabCtrl::SetGroup(AgentGroup& group) {
	this->group = &group;
	
	overview.enable.WhenAction.Clear();
	overview.enable.Set(group.enable_training);
	overview.enable <<= THISBACK(SetEnabled);
	overview.reset_go <<= THISBACK(ResetGroupOptimizer);
	overview.modelist.SetIndex(group.mode);
	
	overview	.SetGroup(group);
	trainingctrl.SetTrainee(group);
	snapctrl	.SetGroup(group);
}

void GroupTabCtrl::Data() {
	int tab = Get();
	if      (tab == 0)
		overview.Data();
	else if (tab == 1)
		trainingctrl.Data();
	else if (tab == 2)
		snapctrl.Data();
}

void GroupTabCtrl::SetEnabled() {
	group->enable_training = overview.enable.Get();
	if (group->enable_training)	group->Start();
	else						group->Stop();
}

void GroupTabCtrl::ResetGroupOptimizer() {
	group->reset_optimizer = true;
}











AgentTabCtrl::AgentTabCtrl() {
	agent = NULL;
	CtrlLayout(overview);
	
	Add(overview);
	Add(overview, "Overview");
	Add(trainingctrl);
	Add(trainingctrl, "Training");
	
	WhenSet << THISBACK(Data);
}

void AgentTabCtrl::Data() {
	if (!agent) return;
	
	int tab = Get();
	
	if (tab == 0) {
		const Symbol& sym = GetMetaTrader().GetSymbol(agent->sym);
		
		String trading_hours;
		for(int i = 0; i < 7; i++) {
			if (i)
				trading_hours << "\n";
			
			trading_hours << Format("%d:%02d - %d:%02d",
				sym.trades_begin_hours[i], sym.trades_begin_minutes[i],
				sym.trades_end_hours[i],   sym.trades_end_minutes[i]);
		}
		overview.opentimes.SetLabel(trading_hours);
		overview.margincur.SetLabel(sym.currency_margin);
		overview.minvolume.SetLabel(DblStr(sym.volume_min));
		overview.peakvalue.SetLabel(DblStr(agent->peak_value));
		overview.bestresult.SetLabel(DblStr(agent->best_result));
		overview.traintime.SetLabel(DblStr(agent->training_time));
		overview.iters.SetLabel(IntStr(agent->iter));
		overview.epsilon.SetLabel(DblStr(agent->dqn.GetEpsilon()));
		
		
		double minimum_margin =
			GetMetaTrader().GetAskBid()[agent->sym].ask *
			sym.volume_min *
			sym.contract_size *
			sym.margin_factor;
		if (sym.IsForex())
			minimum_margin /= GetMetaTrader().AccountLeverage();
		overview.minbasemargin.SetLabel(Format("%2!,n %s", minimum_margin, GetMetaTrader().AccountCurrency()));
	}
	else if (tab == 1) {
		trainingctrl.Data();
	}
}

void AgentTabCtrl::SetAgent(Agent& agent) {
	this->agent = &agent;
	
	trainingctrl.SetTrainee(agent);
}










ManagerCtrl::ManagerCtrl(System& sys) : sys(&sys) {
	view = -1;
	
	CtrlLayout(newview);
	
	Add(hsplit.SizePos());
	
	hsplit.Horz();
	hsplit << ctrl << mainview;
	hsplit.SetPos(1500);
	
	mainview.Add(newview.SizePos());
	mainview.Add(group_tabs.SizePos());
	mainview.Add(agent_tabs.SizePos());
	
	
	String t =
			"{\n"
			"\t\"update\":\"qlearn\",\n"
			"\t\"gamma\":0.9,\n"
			"\t\"epsilon\":0.2,\n"
			"\t\"alpha\":0.005,\n"
			"\t\"experience_add_every\":5,\n"
			"\t\"experience_size\":10000,\n"
			"\t\"learning_steps_per_iteration\":5,\n"
			"\t\"tderror_clamp\":1.0,\n"
			"\t\"num_hidden_units\":100,\n"
			"}\n";
	newview.params.SetData(t);
	newview.create <<= THISBACK(PostNewAgent);
	newview.symlist <<= THISBACK(Data);
	
	
	newview.symlist.AddColumn("");
	newview.symlist.AddColumn("");
	newview.symlist.NoHeader();
	newview.symlist.ColumnWidths("3 1");
	newview.tflist.AddColumn("");
	newview.tflist.AddColumn("");
	newview.tflist.NoHeader();
	newview.tflist.ColumnWidths("3 1");
	newview.all  <<= THISBACK(SelectAll);
	newview.none <<= THISBACK(SelectNone);
	newview.freeze_sig.Set(true);
	newview.allbasefx  <<= THISBACK1(Select, 0);
	newview.allbasecfd <<= THISBACK1(Select, 1);
	newview.allfx      <<= THISBACK1(Select, 2);
	newview.allcfd     <<= THISBACK1(Select, 3);
	newview.allindices <<= THISBACK1(Select, 4);
	newview.allfutures <<= THISBACK1(Select, 5);
	
	
	add_new.SetLabel("New Group");
	add_new   <<= THISBACK1(SetView, 0);
	
	
	ctrl.Add(add_new.TopPos(0, 30).HSizePos());
	ctrl.Add(listsplit.VSizePos(30).HSizePos());
	listsplit.Vert();
	listsplit << glist << alist;
	
	glist.AddColumn("Name");
	glist.AddColumn("Best result");
	glist.AddColumn("Drawdown");
	glist <<= THISBACK1(SetView, 1);
	glist.WhenLeftClick << THISBACK1(SetView, 1);
	
	alist.AddColumn("Symbol");
	alist.AddColumn("Best result");
	alist.AddColumn("Drawdown");
	alist <<= THISBACK1(SetView, 2);
	alist.WhenLeftClick << THISBACK1(SetView, 2);
	
	SetView(0);
}

void ManagerCtrl::SelectAll() {
	for(int i = 0; i < newview.symlist.GetCount(); i++) {
		newview.symlist.Set(i, 1, true);
	}
}

void ManagerCtrl::SelectNone() {
	for(int i = 0; i < newview.symlist.GetCount(); i++) {
		newview.symlist.Set(i, 1, false);
	}
}

void ManagerCtrl::Select(int j) {
	MetaTrader& mt = GetMetaTrader();
	for(int i = 0; i < newview.symlist.GetCount(); i++) {
		const Symbol& s = mt.GetSymbol(i);
		bool sel = false;
		switch (j) {
			case 0: sel = s.IsForex() && s.is_base_currency; break;
			case 1: sel = s.IsCFD()   && s.is_base_currency; break;
			case 2: sel = s.IsForex(); break;
			case 3: sel = s.IsCFD(); break;
			case 4: sel = s.IsCFDIndex(); break;
			case 5: sel = s.IsFuture(); break;
		}
		if (sel)
			newview.symlist.Set(i, 1, true);
	}
}

void ManagerCtrl::SetView(int view) {
	Manager& mgr = sys->GetManager();
	
	newview.Hide();
	agent_tabs.Hide();
	group_tabs.Hide();
	
	if (view == 0) {
		newview.Show();
		newview.SetFocus();
	}
	else if (view == 1) {
		int group_id = glist.GetCursor();
		if (group_id >= 0 && group_id < mgr.groups.GetCount()) {
			group_tabs.SetGroup(mgr.groups[group_id]);
			group_tabs.Show();
			group_tabs.SetFocus();
		}
	}
	else if (view == 2) {
		int group_id = glist.GetCursor();
		if (group_id >= 0 && group_id < mgr.groups.GetCount()) {
			AgentGroup& group = mgr.groups[group_id];
			int agent_id = alist.GetCursor();
			if (agent_id >= 0 && agent_id < group.agents.GetCount()) {
				agent_tabs.SetAgent(group.agents[agent_id]);
				agent_tabs.Show();
				agent_tabs.SetFocus();
			}
		}
	}
	this->view = view;
	Data();
}

void ManagerCtrl::Data() {
	Manager& mgr = sys->GetManager();
	
	for(int i = 0; i < mgr.groups.GetCount(); i++) {
		AgentGroup& g = mgr.groups[i];
		
		glist.Set(i, 0, g.name);
		glist.Set(i, 1, g.best_result);
		glist.Set(i, 2, g.last_drawdown);
	}
	glist.SetCount(mgr.groups.GetCount());
	
	int gcursor = glist.GetCursor();
	
	if (gcursor >= 0 && gcursor < mgr.groups.GetCount()) {
		AgentGroup& g = mgr.groups[gcursor];
		
		for(int i = 0; i < g.agents.GetCount(); i++) {
			Agent& a = g.agents[i];
			
			alist.Set(i, 0, a.sym != -1 ? sys->GetSymbol(a.sym) : "");
			alist.Set(i, 1, a.best_result);
			alist.Set(i, 2, a.last_drawdown);
		}
		alist.SetCount(g.agents.GetCount());
	}
	
	if (view == 0) {
		// Set symbol and tf lists if empty
		if (newview.tflist.GetCount() == 0) {
			for(int i = 0; i < sys->GetPeriodCount(); i++) {
				newview.tflist.Set(i, 0, sys->GetPeriodString(i));
				newview.tflist.Set(i, 1, 0);
				newview.tflist.SetCtrl(i, 1, new_opts.Add());
			}
		}
		if (newview.symlist.GetCount() == 0) {
			for(int i = 0; i < sys->GetBrokerSymbolCount(); i++) {
				newview.symlist.Set(i, 0, sys->GetSymbol(i));
				newview.symlist.Set(i, 1, 0);
				newview.symlist.SetCtrl(i, 1, new_opts.Add());
			}
		}
	}
	else if (view == 1) {
		group_tabs.Data();
	}
	else if (view == 2) {
		agent_tabs.Data();
	}
}

void ManagerCtrl::NewAgent() {
	Manager& mgr = sys->GetManager();
	
	One<AgentGroup> group_;
	group_.Create();
	AgentGroup& group = *group_;
	
	group.name = newview.name.GetData();
	if (group.name == "") {
		PromptOK(DeQtf("Set name"));
		return;
	}
	
	for(int i = 0; i < mgr.groups.GetCount(); i++) {
		if (group.name == mgr.groups[i].name) {
			PromptOK(DeQtf("An agent with the name " + group.name + " exists already."));
			return;
		}
	}
	
	// Timeframes
	for(int i = newview.tflist.GetCount()-1; i >= 0; i--) {
		if (newview.tflist.Get(i, 1))
			group.tf_ids.Add(i);
	}
	if (group.tf_ids.IsEmpty()) {
		PromptOK(DeQtf("At least one timeframe must be selected"));
		return;
	}
	
	
	// Symbols
	for(int i = 0; i < newview.symlist.GetCount(); i++) {
		if (newview.symlist.Get(i, 1))
			group.sym_ids.Add(i);
	}
	if (group.sym_ids.IsEmpty()) {
		PromptOK(DeQtf("At least one symbol must be selected"));
		return;
	}
		
	
	group.sig_freeze				= newview.freeze_sig.Get();
	group.param_str					= newview.params.GetData();
	group.accum_signal				= newview.accum_signal.Get();
	
	group.sys = sys;
	
	mgr.groups.Add(group_.Detach());
	
	PostCallback(THISBACK(Data));
	PostCallback(THISBACK(LastCursor));
	
	
	group.Init();
	group.StoreThis();
	group.Start();
}
















RealtimeCtrl::RealtimeCtrl(System& sys) :
	sys(&sys)
{
	Add(hsplit.SizePos());
	hsplit.Horz();
	hsplit << brokerctrl << journal;
	hsplit.SetPos(8000);
	
	
	journal.AddColumn("Time");
	journal.AddColumn("Level");
	journal.AddColumn("Message");
	journal.ColumnWidths("3 1 3");
	
	
	brokerctrl.SetBroker(GetMetaTrader());
	
	sys.WhenRealtimeUpdate << THISBACK(PostData);
}

void RealtimeCtrl::AddMessage(String time, String level, String msg) {
	journal.Insert(0);
	journal.Set(0, 0, time);
	journal.Set(0, 1, level);
	journal.Set(0, 2, msg);
}

void RealtimeCtrl::Info(String msg) {
	PostCallback(THISBACK3(AddMessage,
		Format("%", GetSysTime()),
		"Info",
		msg));
}

void RealtimeCtrl::Error(String msg) {
	PostCallback(THISBACK3(AddMessage,
		Format("%", GetSysTime()),
		"Error",
		msg));
}

void RealtimeCtrl::Data() {
	MetaTrader& mt = GetMetaTrader();
	mt.ForwardExposure();
	
	brokerctrl.Data();
}

void RealtimeCtrl::Init() {
	MetaTrader& mt = GetMetaTrader();
	mt.WhenInfo  << THISBACK(Info);
	mt.WhenError << THISBACK(Error);
	
	sys->WhenInfo  << THISBACK(Info);
	sys->WhenError << THISBACK(Error);
}

}