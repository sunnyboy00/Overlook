#ifndef _Overlook_DQN_h_
#define _Overlook_DQN_h_

#include <random>
#include "RandomAMP.h"

namespace Overlook {

class RandomGaussian {
	std::default_random_engine generator;
	std::normal_distribution<double> distribution;
	
public:

	// weight normalization is done to equalize the output
	// variance of every neuron, otherwise neurons with a lot
	// of incoming connections have outputs of larger variance
	RandomGaussian(int length) : distribution(0, sqrt(1.0 / (double)(length))) {
		generator.seed(Random(1000000000));
	}
	double Get() {return distribution(generator);}
	operator double() {return distribution(generator);}
	
};

template <class T> // Workaround for GCC bug - specialization needed...
T& SingleRandomGaussianLock() {
	static T o;
	return o;
}

inline RandomGaussian& GetRandomGaussian(int length) {
	SpinLock& lock = SingleRandomGaussianLock<SpinLock>(); // workaround
	ArrayMap<int, RandomGaussian>& rands = Single<ArrayMap<int, RandomGaussian> >();
	lock.Enter();
	int i = rands.Find(length);
	RandomGaussian* r;
	if (i == -1) {
		r = &rands.Add(length, new RandomGaussian(length));
	} else {
		r = &rands[i];
	}
	lock.Leave();
	return *r;
}


//speed up the memcpy process!
template <class T>
void copy_linear(T* write, const T* read, unsigned int size) PARALLEL {
	int* w4 = (int*)write;
	int* r4 = (int*)read;
	unsigned int scan=0;
	
	AMPASSERT(size % sizeof(int) == 0);
	
	while (size >= 4) {
		w4[scan] = r4[scan];
		size -= 4;
		scan++;
	}
}

#define COPY(type, dst, src, count) copy_linear(dst, src, sizeof(type) * count)
#define ZERO(dst, count) {for(int i = 0; i < count; i++) dst[i] = 0;}
#define SET(dst, count, value) {for(int i = 0; i < count; i++) dst[i] = value;}
#define RAND(dst, count) {RandomGaussian& rand = GetRandomGaussian(count);	for (int i = 0; i < count; i++) {dst.Set(i, rand);}}











template <class T, int width, int height>
class Mat : Moveable<Mat<T, width, height> > {
	static const int length = width * height;
	
	T weight_gradients[length];
	T weights[length];
	
	typedef Mat<T, width, height> MatType;
	
protected:
	inline int GetPos(int x, int y) const PARALLEL {
		AMPASSERT(x >= 0 && y >= 0 && x < width && y < height);
		return (width * y) + x;
	}
	inline int Pos(int i) const PARALLEL {
		AMPASSERT(i >= 0 && i < length);
		return i;
	}
	
public:
	
	Mat() {}
	Mat(MatType& vol) {COPY(T, this->weights, vol.weights, length); ZERO(weight_gradients, length);}
	Mat(const T weights[length]) {COPY(T, this->weights, weights, length); ZERO(weight_gradients, length);}
	Mat(const MatType& o) {*this = o;}
	Mat(T default_value) {SET(weights, length, default_value); ZERO(weight_gradients, length);}
	Mat& Init() PARALLEL {RAND(weights, length); ZERO(weight_gradients, length); return *this;}
	Mat& Init(const T weights[length]) PARALLEL {COPY(T, this->weights, weights, length); ZERO(weight_gradients, length); return *this;}
	Mat& Init(T default_value) PARALLEL {SET(weights, length, default_value); ZERO(weight_gradients, length); return *this;}
	
	void Serialize(Stream& s) {
		if (s.IsStoring()) {
			s.Put(weights,				sizeof(T) * length);
			s.Put(weight_gradients,		sizeof(T) * length);
		}
		else if (s.IsLoading()) {
			s.Get(weights,				sizeof(T) * length);
			s.Get(weight_gradients,		sizeof(T) * length);
		}
	}
	
	Mat& operator=(const MatType& src) PARALLEL {COPY(T, this->weights, src.weights, length); COPY(T, this->weight_gradients, src.weight_gradients, length); return *this;}
	
	const T* GetWeights()   const PARALLEL {return weights;}
	const T* GetGradients() const PARALLEL {return weight_gradients;}
	
	void Add(int i, T v) PARALLEL {weights[Pos(i)] += v;}
	void Add(int x, int y, T v) PARALLEL {weights[GetPos(x,y)] += v;}
	void AddFrom(const MatType& volume) PARALLEL {for (int i = 0; i < length; i++) {weights[i] += volume.weights[i];}}
	void AddFromScaled(const Mat& volume, T a) PARALLEL {for (int i = 0; i < length; i++) {weights[i] += a * volume.weights[i];}}
	void AddGradient(int x, int y, T v) PARALLEL {weight_gradients[GetPos(x,y)] += v;}
	void AddGradient(int i, T v) PARALLEL {weight_gradients[Pos(i)] += v;}
	void AddGradientFrom(const Mat& volume) PARALLEL {for (int i = 0; i < length; i++) {weight_gradients[i] += volume.weight_gradients[i];}}
	T Get(int x, int y) const PARALLEL {return weights[GetPos(x, y)];}
	T GetGradient(int x, int y) const PARALLEL {return weight_gradients[GetPos(x, y)];}
	void Set(int x, int y, T v) PARALLEL {weights[GetPos(x, y)] = v;}
	void SetConst(T c) PARALLEL {for (int i = 0; i < length; i++) {weights[i] = c;}}
	void SetConstGradient(T c) PARALLEL {for (int i = 0; i < length; i++) {weight_gradients[i] = c;}}
	void SetGradient(int x, int y, T v) PARALLEL {weight_gradients[GetPos(x, y)] = v;}
	T Get(int i) const PARALLEL {return weights[Pos(i)];}
	void Set(int i, T v) PARALLEL {weights[Pos(i)] = v;}
	T GetGradient(int i) const PARALLEL {return weight_gradients[Pos(i)];}
	void SetGradient(int i, T v) PARALLEL {weight_gradients[Pos(i)] = v;}
	void ZeroGradients() PARALLEL {for (int i = 0; i < length; i++) {weight_gradients[i] = 0;}}
	
	int GetWidth() const PARALLEL {return width;}
	int GetHeight() const PARALLEL {return height;}
	int GetLength() const PARALLEL {return length;}
	
	int GetMaxColumn() const PARALLEL {
		T max = -DBL_MAX;
		int pos = -1;
		for(int i = 0; i < length; i++) {
			T d = weights[i];
			if (i == 0 || d > max) {
				max = d;
				pos = i;
			}
		}
		return pos;
	}
	
	int GetSampledColumn() const PARALLEL {
		// sample argmax from w, assuming w are
		// probabilities that sum to one
		T r = Randomf();
		T x = 0.0;
		for(int i = 0; i < length; i++) {
			x += weights[i];
			if (x > r) {
				return i;
			}
		}
		return length - 1;
	}
	
	
	
};

















inline double sig(double x) {
	// helper function for computing sigmoid
	return 1.0 / (1.0 + exp(-x));
}

template <int WIDTH, int HEIGHT>
struct RecurrentTanh {
	typedef Mat<double, WIDTH, HEIGHT> MatType;
	static const int length = WIDTH * HEIGHT;
	MatType output;
	
	template <class T>
	MatType& Forward(const T& input) PARALLEL {
		AMPASSERT(input.GetWidth() == WIDTH && input.GetHeight() == HEIGHT);
		// tanh nonlinearity
		output.Init(0.0);
		for (int i = 0; i < length; i++) {
			output.Set(i, AmpTanh(input.Get(i)));
		}
		
		return output;
	}
	
	template <class T>
	void Backward(T& input) PARALLEL {
		AMPASSERT(input.GetWidth() == WIDTH && input.GetHeight() == HEIGHT);
		for (int i = 0; i < length; i++) {
			// grad for z = Tanh(x) is (1 - z^2)
			double mwi = output.Get(i);
			double d = (1.0 - mwi * mwi) * output.GetGradient(i);
			input.AddGradient(i, d);
		}
	}
	
	void Serialize(Stream& s) {s % output;}
	
};

template <int WIDTH, int HEIGHT>
struct RecurrentMul {
	typedef Mat<double, WIDTH, HEIGHT> MatType;
	static const int length = WIDTH * HEIGHT;
	MatType output;
	
	template <class T1, class T2>
	MatType& Forward(const T1& input1, const T2& input2) PARALLEL {
		
		// multiply matrices input1 * input2
		AMPASSERT_(input1.GetWidth() == input2.GetHeight(), "matmul dimensions misaligned");
		
		int h = input1.GetHeight();
		int w = input2.GetWidth();
		AMPASSERT(WIDTH == w && HEIGHT == h);
		output.Init(0.0);
		
		// loop over rows of input1
		for (int i = 0; i < h; i++) {
			
			// loop over cols of input2
			for (int j = 0; j < w; j++) {
				
				// dot product loop
				double dot = 0.0;
				for (int k = 0; k < input1.GetWidth(); k++) {
					dot += input1.Get(k, i) * input2.Get(j, k);
				}
				output.Set(j, i, dot);
			}
		}
		return output;
	}
	
	template <class T1, class T2>
	void Backward(T1& input1, T2& input2) PARALLEL {
		
		// loop over rows of m1
		for (int i = 0; i < input1.GetHeight(); i++) {
			
			// loop over cols of m2
			for (int j = 0; j < input2.GetWidth(); j++) {
				
				// dot product loop
				for (int k = 0; k < input1.GetWidth(); k++) {
					double b = output.GetGradient(j, i);
					input1.AddGradient(k, i, input2.Get(j, k) * b);
					input2.AddGradient(j, k, input1.Get(k, i) * b);
				}
			}
		}
	}
	
	void Serialize(Stream& s) {s % output;}
	
};

template <int WIDTH, int HEIGHT>
struct RecurrentAdd {
	typedef Mat<double, WIDTH, HEIGHT> MatType;
	static const int length = WIDTH * HEIGHT;
	MatType output;
	
	template <class T1, class T2>
	MatType& Forward(const T1& input1, const T2& input2) PARALLEL {
		AMPASSERT(input1.GetLength() == input2.GetLength());
		AMPASSERT(output.GetWidth() == input1.GetWidth() && output.GetHeight() == input1.GetHeight());
		
		output.Init(0.0);
		for (int i = 0; i < input1.GetLength(); i++) {
			output.Set(i, input1.Get(i) + input2.Get(i));
		}
		
		return output;
	}
	
	template <class T1, class T2>
	void Backward(T1& input1, T2& input2) PARALLEL {
		for (int i = 0; i < input1.GetLength(); i++) {
			input1.AddGradient(i, output.GetGradient(i));
			input2.AddGradient(i, output.GetGradient(i));
		}
	}
	
	void Serialize(Stream& s) {s % output;}
	
};






template <int WIDTH, int HEIGHT>
struct DQExperience : Moveable<DQExperience<WIDTH, HEIGHT> > {
	typedef Mat<float, WIDTH, HEIGHT> MatType;
	MatType state0, state1;
	int action0, action1;
	double reward0;
	
	void Set(MatType& state0, int action0, double reward0, MatType& state1, int action1) PARALLEL {
		this->state0 = state0;
		this->action0 = action0;
		this->reward0 = reward0;
		this->state1 = state1;
		this->action1 = action1;
	}
	void Serialize(Stream& s) {s % state0 % state1 % action0 % action1 % reward0;}
};





// return Mat but filled with random numbers from gaussian
template <class T>
void RandMat(double mu, double std, T& m) {
	m.Init(0.0);
	
	std::default_random_engine generator;
	std::normal_distribution<double> distribution(mu, std);
	generator.seed(Random(INT_MAX));
	
	for (int i = 0; i < m.GetLength(); i++)
		m.Set(i, distribution(generator));
}

/*
int SampleWeighted(Vector<double>& p) {
	ASSERT(!p.IsEmpty());
	double r = Randomf();
	double c = 0.0;
	for (int i = 0; i < p.GetCount(); i++) {
		c += p[i];
		if (c >= r)
			return i;
	}
	Panic("Invalid input vector");
	return 0;
}*/


template <class MatType>
void UpdateMat(MatType& m, double alpha) PARALLEL {
	// updates in place
	for (int i = 0; i < m.GetLength(); i++) {
		double d = m.GetGradient(i);
		m.Add(i, -alpha * d);
		m.SetGradient(i, 0);
	}
}



template <int num_actions, int num_states, int num_hidden_units = 100, int experience_size=100>
class DQNAgent {
	
public:
	
	typedef Mat<float, 1, num_states> MatType;
	
	Mat<double, num_states, num_hidden_units>			W1;
	Mat<double, 1, num_hidden_units>					b1;
	Mat<double, num_hidden_units, num_actions>			W2;
	Mat<double, 1, num_actions>							b2;
	
	// width = agent-width * agent-height, height = W1.height
	RecurrentMul<1, num_hidden_units>					mul1;
	RecurrentAdd<1, num_hidden_units>					add1;
	RecurrentTanh<1, num_hidden_units>					tanh;
	
	// width = tanh.width, height = W2.height
	RecurrentMul<1, num_actions>						mul2;
	RecurrentAdd<1, num_actions>						add2;
	
	typedef  Mat<double, 1, num_actions>				FwdOut;
	typedef  DQExperience<1, num_states>				DQExp;
	
protected:
	DQExp exp[experience_size]; // experience
	double gamma, epsilon, alpha, tderror_clamp;
	double tderror;
	int experience_add_every;
	int learning_steps_per_iteration;
	int expi;
	int t;
	int exp_count;
	
	bool has_reward;
	MatType state;
	MatType state0, state1;
	int action0, action1;
	double reward0;
	
	
public:

	DQNAgent() {
		gamma = 0.9;	// future reward discount factor
		epsilon = 0.02;	// for epsilon-greedy policy
		alpha = 0.005;	// value function learning rate
		
		experience_add_every = 5; // number of time steps before we add another experience to replay memory
		learning_steps_per_iteration = 5;
		tderror_clamp = 1.0;
		
		expi = 0;
		t = 0;
		reward0 = 0;
		action0 = 0;
		action1 = 0;
		has_reward = false;
		tderror = 0;
		
		exp_count = 0;
	}
	
	void Reset() {
		RandMat(0, 0.01, W1);
		b1.Init(0.0);
		RandMat(0, 0.01, W2);
		b2.Init(0.0);
		
		expi = 0; // where to insert
		
		t = 0;
		
		reward0 = 0;
		action0 = 0;
		action1 = 0;
		has_reward = false;
		
		tderror = 0; // for visualization only...
	}
	
	FwdOut& Forward(MatType& input) PARALLEL {
		// Original:
		//		var a1mat = G.Add(G.Mul(net.W1, s), net.b1);
		//		var h1mat = G.Tanh(a1mat);
		//		var a2mat = G.Add(G.Mul(net.W2, h1mat), net.b2);
		mul1.Forward(W1,			input);
		add1.Forward(mul1.output,	b1);
		tanh.Forward(add1.output);
		mul2.Forward(W2,			tanh.output);
		add2.Forward(mul2.output,	b2);
		
		return add2.output;
	}
	
	void Backward(MatType& input) PARALLEL {
		add2.Backward(mul2.output,	b2);
		mul2.Backward(W2,			tanh.output);
		tanh.Backward(add1.output);
		add1.Backward(mul1.output,	b1);
		mul1.Backward(W1,			input);
	}
	
	int Act(float slist[num_states], tinymt& rand) PARALLEL {
		
		// convert to a Mat column vector
		state.Init(slist);
		
		
		// epsilon greedy policy
		int action;
		if (rand.next_single() < epsilon) {
			action = rand.next_uint() % num_actions;
		} else {
			// greedy wrt Q function
			//Mat& amat = ForwardQ(net, state);
			FwdOut& amat = Forward(state);
			action = amat.GetMaxColumn(); // returns index of argmax action
		}
		
		
		// shift state memory
		state0 = state1;
		action0 = action1;
		state1 = state;
		action1 = action;
		
		
		return action;
	}
	
	void Learn(double reward1, tinymt& rand) PARALLEL {
		
		// perform an update on Q function
		if (has_reward && alpha > 0 && state0.GetLength() > 0) {
			
			// learn from this tuple to get a sense of how "surprising" it is to the agent
			tderror = LearnFromTuple(state0, action0, reward0, state1, action1); // a measure of surprise
			
			// decide if we should keep this experience in the replay
			if (t % experience_add_every == 0) {
				if (exp_count == expi)
					exp_count++;
				AMPASSERT(state1.GetLength() > 0);
				exp[expi].Set(state0, action0, reward0, state1, action1);
				expi += 1;
				if (expi >= experience_size) { expi = 0; } // roll over when we run out
			}
			t += 1;
			
			if (exp_count) {
				// sample some additional experience from replay memory and learn from it
				for (int k = 0; k < learning_steps_per_iteration; k++) {
					int ri = rand.next_uint() % exp_count; // TODO: priority sweeps?
					DQExp& e = exp[ri];
					LearnFromTuple(e.state0, e.action0, e.reward0, e.state1, e.action1);
				}
			}
		}
		reward0 = reward1; // store for next update
		has_reward = true;
	}
	
	double LearnFromTuple(MatType& s0, int a0, double reward0, MatType& s1, int a1) PARALLEL {
		
		AMPASSERT(s0.GetLength() > 0);
		AMPASSERT(s1.GetLength() > 0);
		// want: Q(s,a) = r + gamma * max_a' Q(s',a')
		
		// compute the target Q value
		FwdOut& tmat = Forward(s1);
		double qmax = reward0 + gamma * tmat.Get(tmat.GetMaxColumn());
		
		// now predict
		FwdOut& pred = Forward(s0);
		
		double tderror = pred.Get(a0) - qmax;
		double clamp = tderror_clamp;
		double abs_tderror = tderror >= 0.0 ? +tderror : -tderror;
		if (abs_tderror > clamp) {  // huber loss to robustify
			if (tderror > clamp)
				tderror = +clamp;
			else
				tderror = -clamp;
		}
		pred.SetGradient(a0, tderror);
		Backward(s0); // compute gradients on net params
		
		// update net
		UpdateMat(W1, alpha);
		UpdateMat(b1, alpha);
		UpdateMat(W2, alpha);
		UpdateMat(b2, alpha);
		
		return tderror;
	}
	
	int GetExperienceWritePointer() const PARALLEL {return expi;}
	double GetTDError() const PARALLEL {return tderror;}
	double GetEpsilon() const PARALLEL {return epsilon;}
	
	int GetExperienceCount() const PARALLEL {return experience_size;}
	
	void SetEpsilon(double e) {epsilon = e;}
	
	
	void Serialize(Stream& s) {
		s % W1 % b1 % W2 % b2 % mul1 % add1 % tanh % mul2 % add2;
		for(int i = 0; i < experience_size; i++)
			s % exp[i];
		s % gamma % epsilon % alpha % tderror_clamp % tderror % experience_add_every
		  % learning_steps_per_iteration % expi % t % exp_count;
	}
};

}

#endif