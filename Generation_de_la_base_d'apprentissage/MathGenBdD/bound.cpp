
using namespace std;

#include "ilcplex\ilocplex.h"
ILOSTLBEGIN

#include <iomanip>
#include <iostream>
#include <vector>
#include<list>
#include <algorithm>

#define MAX_JOBS 600

double sp_time;
double avg_sp_time=0;
double max_sp_time=0;

int initial_obj;

int sp_cnt=0;

struct Job {
  int id;
  int ptime[2];
};


void 
get_sequence(IloCplex cpx, IloIntVarArray x[], const vector<Job>& N, vector<int>& S)
{
  for(int i=0; i<N.size(); ++i) {
    for(int j=0; j<N.size(); ++j) {
      if( cpx.getValue(x[i][j])>.5 ) {
	  S[j]=N[i].id;
      }
    }
  }
}


bool 
search(IloCplex cpx, IloIntVarArray x[], vector<Job>& N, vector<int>& S, int&bestobj, double timelim=1e20)
{
    const int wsize=12;
    const int max_Nr_win=N.size()-wsize;

    vector<bool> used(N.size());

    bool done=false;
    cout<<"initial="<<bestobj<<endl;

    clock_t start_time=clock();
    
    while( !done ) {
	done=true;
	for(int i=0; i<max_Nr_win && done;++i) 
	    done= done && used[i];
	if( !done ) {
	    int wstart;
	    list<pair<int, int> > L;
	    do {
		wstart=rand()%max_Nr_win;
	    } while(used[wstart]);
	    for(int i=0; i<N.size(); i++) {
		if( i<wstart || i>wstart+wsize-1 ) {
		  x[S[i]][i].setLB(1);
		  x[S[i]][i].setUB(1);
		  L.push_back(pair<int, int>(S[i],i));
		}
	    }
            clock_t sp_start=clock();
	    cpx.solve();
            sp_time=((double)(clock()-sp_start)/CLOCKS_PER_SEC);
            avg_sp_time+=sp_time;
            sp_cnt++;
            if( sp_time>max_sp_time ) {
              max_sp_time=sp_time;
            }
	    if( cpx.getObjValue()<bestobj ) {
		bestobj=(int) cpx.getObjValue();
		get_sequence(cpx, x, N, S);
		cout<<"Improved to "<<bestobj<<
                  " at time "<< ((double)(clock()-start_time))/CLOCKS_PER_SEC << endl;
		fill(used.begin(), used.end(), false);
	    }
	    while( !L.empty() ) {
	      x[L.front().first][L.front().second].setLB(0);
	      x[L.front().first][L.front().second].setUB(1);
	      L.pop_front();
	    }
	}
	clock_t stop_time=clock();
	if( ((stop_time-start_time)/CLOCKS_PER_SEC)>timelim )
	    break;
    }
}

void 
read_jobs(const char *fname, vector<Job>& N)
{
  int dummy;
  N.resize(0);
  ifstream ifs(fname);
  if( ifs.is_open() ) {
    Job buf;
    while( !ifs.eof() ) {
      ifs>>buf.id;
      buf.id--;
      ifs>>buf.ptime[0]>>buf.ptime[1];
      ifs>>dummy;
      //      cout<<"Here"<<endl;
      //      cout<<buf.ptime[0]<<"\t"<<buf.ptime[1]<<endl;
      if( !ifs.eof() )
	N.push_back(buf);
    }
  }
  else {
    char msg[128];
    snprintf(msg, 127, "read_jobs(): cannot open %s", fname );
    throw(msg);
  }
}


int 
read_sequence(const char *fname, vector<int>& S)
{
    ifstream ifs(fname);
    int obj;

    if( ifs.is_open() ) {
	ifs>>obj;
	for(int i=0; i<S.size(); ++i)
	    ifs>>S[i];
    }
    else {
	char buf[128];
	snprintf(buf, 127, "read_sequence(): cannot open %s", fname);
	throw(buf);
    }
    return obj;
}


int main(int argc, char *argv[])
{
  int res=EXIT_SUCCESS;
  try {
    if( argc<4 ) {
      throw("Please provide file name and random seed");
    }

    srand(atoi(argv[2]));
    double tlim=atof(argv[3]);
    vector<Job> N;
    read_jobs(argv[1], N);

    cout<<N.size()<<" jobs"<<endl;

    IloEnv env;
    IloModel model(env);

    IloIntVarArray x[MAX_JOBS];
    for(int i=0; i<N.size(); ++i) {
      char buf[128];
      x[i]=IloIntVarArray(env, N.size(), 0.0, 1.0);
      for(int j=0; j<N.size(); ++j) {
	snprintf(buf, 127, "x(%d,%d)", i, j);
	x[i][j].setName(buf);
      }
    }

    IloNumVarArray Ctime[2];
    Ctime[0]=IloNumVarArray(env, N.size(), 0.0, IloInfinity);
    Ctime[1]=IloNumVarArray(env, N.size(), 0.0, IloInfinity);
    for(int i=0; i<N.size(); ++i) {
      char buf[128];
      snprintf(buf, 127, "C(0,%d)", i);
      //cout<<buf<<endl;
      Ctime[0][i].setName(buf);
      snprintf(buf, 127, "C(1,%d)", i);
      //cout<<buf<<endl;
      Ctime[1][i].setName(buf);
    }

    //cout<<Ctime[1]<<endl;

    model.add(IloMinimize(env, IloSum(Ctime[1])));

    for(int i=0; i<N.size(); ++i) {
      IloExpr e(env);
      for(int j=0; j<N.size(); ++j) {
	e+=x[i][j];
      }
      model.add(e==1.0);
      e.end();
    }

    for(int j=0; j<N.size(); ++j) {
      IloExpr e(env);
      for(int i=0; i<N.size(); ++i) {
	e+=x[i][j];
      }
      model.add(e==1.0);
    }


    for(int pos=0; pos<N.size(); ++pos) {
      IloExpr tmp(env), tmp2(env), tmp3(env);
      tmp.clear();
      tmp2.clear();
      tmp3.clear();
      for(int i=0; i<N.size(); ++i) {
	tmp+=N[i].ptime[0]*x[i][pos];
	tmp2+=N[i].ptime[1]*x[i][pos];
	tmp3+=N[i].ptime[1]*x[i][pos];
      }
      if( !pos ) {
	model.add(Ctime[0][0]>=tmp);
	model.add(Ctime[1][0]>=Ctime[0][0]+tmp2);
      }
      else {
	model.add(Ctime[0][pos]>=Ctime[0][pos-1]+tmp);
	model.add(Ctime[1][pos]>=Ctime[1][pos-1]+tmp2);
	model.add(Ctime[1][pos]>=Ctime[0][pos]+tmp3);
      }
    }
    IloCplex cpx(model);

    cpx.exportModel("pippo.lp");
    cpx.setParam(IloCplex::TiLim, tlim);
    cpx.setParam(IloCplex::MIPDisplay, 0);
    cpx.setParam(IloCplex::EpGap, 0);
    /*
    cpx.solve();
    int cur_best_obj=(int) cpx.getObjValue();
    */
    cpx.solve();
    cout<<"BOUND="<<fixed<<cpx.getBestObjValue()<<endl;
    cout<<"BESTOBJ="<<fixed<<cpx.getObjValue()<<endl;
  }

  catch(const char *s) {
    cerr<<s<<endl;
    res=EXIT_FAILURE;
  }
  catch(IloException e) {
    cerr<<e<<endl;
    res=EXIT_FAILURE;
  }
  catch(...) {
    cerr<<"Unhandled exception, my friend!"<<endl;
    res=EXIT_FAILURE;
  }

  return res;
}
