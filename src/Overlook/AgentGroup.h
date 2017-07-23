#ifndef _Overlook_AgentGroup_h_
#define _Overlook_AgentGroup_h_

namespace Overlook {

class AgentGroup : public TraineeBase {
	
public:
	
	// Persistent
	ConvNet::DQNAgent dqn;
	Array<Agent> agents;
	Vector<Vector<int> > train_pos;
	Vector<int> train_pos_all;
	Index<int> tf_ids, sym_ids;
	Time created;
	String name;
	String param_str;
	double fmlevel;
	double limit_factor;
	int agent_input_width, agent_input_height;
	int group_input_width, group_input_height;
	int mode;
	bool sig_freeze;
	bool enable_training;
	bool accum_signal;
	
	// Temp
	Vector<Vector<Vector<ConstBuffer*> > > value_buffers;
	Vector<Ptr<CoreItem> > work_queue, db_queue;
	Vector<Core*> databridge_cores;
	Array<Snapshot> snaps;
	Vector<double> input_values;
	Vector<int> data_begins;
	Vector<int> tf_periods;
	Index<int> indi_ids;
	TimeStop last_store;
	double prev_equity;
	int fastest_period_mins;
	int buf_count;
	int data_size, signal_size, total_size;
	int act_iter;
	int main_tf, main_tf_pos;
	bool reset_optimizer;
	bool allow_realtime;
	bool is_looping;
	System* sys;
	Mutex work_lock;
	
	
public:
	typedef AgentGroup CLASSNAME;
	AgentGroup();
	~AgentGroup();
	void Init();
	void Start();
	void StartGroup();
	void StartAgents();
	void Stop();
	void StopGroup();
	void StopAgents();
	void Main();
	virtual void Create(int width, int height);
	virtual void Forward(Snapshot& snap, SimBroker& broker, Snapshot* next_snap=NULL) {Forward(snap, (Brokerage&)broker, next_snap);}
	virtual void Backward(double reward);
	void Forward(Snapshot& snap, Brokerage& broker, Snapshot* next_snap=NULL);
	void StoreThis();
	void LoadThis();
	void Serialize(Stream& s);
	void GenerateSnapshots();
	void RefreshSnapshots();
	void ResetSnapshot(Snapshot& snap);
	bool Seek(Snapshot& snap, int shift);
	void RefreshWorkQueue();
	void ProcessWorkQueue();
	void ProcessDataBridgeQueue();
	void ResetValueBuffers();
	void InitThreads();
	void CreateAgents();
	void Progress(int actual, int total, String desc);
	void SubProgress(int actual, int total);
	void SetEpsilon(double d);
	void SetMode(int i);
	bool PutLatest(Brokerage& broker);
	virtual void SetAskBid(SimBroker& sb, int pos);
	void LoopAgentsToEnd();
	void LoopAgentToEnd(int i);
	
	int GetSignalBegin() const;
	int GetSignalEnd() const;
	int GetSignalPos(int group_id) const;
	
	Callback3<int, int, String> WhenProgress;
	Callback2<int, int> WhenSubProgress;
	int a0, t0, a1, t1;
	String prog_desc;
	
	Callback1<String> WhenInfo;
	Callback1<String> WhenError;
};

}

#endif
