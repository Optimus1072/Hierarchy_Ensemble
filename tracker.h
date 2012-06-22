#ifndef TRACKER_H
#define TRACKER_H

#include <list>

#include "opencv2/opencv.hpp"

#include "appTemplate.h"
#include "parameter.h"
#include "util.h"

using namespace cv;
using namespace std;


class EnsembleTracker
{

public:
	EnsembleTracker(int id,Size body_size,double phi1=0.5*AUX_phi1,double phi2=1.5*AUX_phi2,double phi_max=4.0);		
	~EnsembleTracker();	

	//reference counting
	inline void refcAdd1(){	++_refc;	}
	inline void refcDec1(){--_refc;_refc=MAX(0,_refc);}
	inline size_t getRefc(){return _refc;}

	// memory management
	inline bool getIsDumped(){return _is_dumped;}
	void dump();
	static void emptyTrash();

	// major functions
	void updateNeighbors(
		list<EnsembleTracker*> tr_list,
		double dis_thresh_r=2.5, // ratio of distance thresh to '_match_radius'
		double scale_r1=1.2, double scale_r2=0.8,
		double hist_thresh=0.5);
	void addAppTemplate(const Mat* frame_set,Rect iniWin);
	void track(const Mat* frame_set,Mat& occ_map);
	void calcConfidenceMap(const Mat* frame_set, Mat& occ_map);//using kalman filter to decide the window
	void calcScore();//calculate each template's score
	void deletePoorTemplate(double threshold);
	void deletePoorestTemplate();		
	void demote();
	void promote();
	void registerTrackResult();//record result window and the filtered one

	//for auxiliary stable appearance
	void updateMatchHist(Mat& frame);
	double compareHisto(Mat& frame, Rect win);
	
	inline double getVel()//get velocity
	{
		return (abs(_kf.statePost.at<float>(2,0))+abs(_kf.statePost.at<float>(3,0)))*_FRAME_RATE;
	}
	inline void setAddNew(bool b){_added_new=b;}
	inline bool getAddNew(){return _added_new;}
	inline double getHitFreq()
	{
		Scalar s=sum(_recentHitRecord.row(0));
		return s[0]/MIN((double)_recentHitRecord.cols,_result_history.size());		
	}
	inline double getHitMeanScore()
	{
		return mean(_recentHitRecord.row(1))[0];
	}
	
	//drawing function
	inline void drawFilterWin(Mat& frame)
	{
		//Rect win=_filter_result_history.back();
		//rectangle(frame,win,COLOR(_ID),1);
	}
	inline void drawResult(Mat& frame,double scale)
	{
		scale-=1;
		Rect win=_result_history.back();//_result_history.back();
		rectangle(frame,scaleWin(win,1/TRACKING_TO_GT_RATIO),COLOR(_ID),2);
	}
	inline void drawAssRadius(Mat& frame)
	{
		Rect win=_result_temp;
		circle(frame,Point((int)(win.x+0.5*win.width),(int)(win.y+0.5*win.height)),(int)_match_radius,COLOR(_ID),1);
	}

	//getting function
	inline double getAssRadius(){return _match_radius;}	
	inline int getTemplateNum(){return _template_list.size();}
	inline vector<Rect>& getResultHistory(){return _result_history;}
	inline int getID(){return _ID;}
	inline double getDisToLast(Rect win) // to last position of non-novice status
	{
		return sqrt(
			(win.x+0.5*win.width-_result_last_no_sus.x-0.5*_result_last_no_sus.width)*(win.x+0.5*win.width-_result_last_no_sus.x-0.5*_result_last_no_sus.width)+
			(win.y+0.5*win.height-_result_last_no_sus.y-0.5*_result_last_no_sus.height)*(win.y+0.5*win.height-_result_last_no_sus.y-0.5*_result_last_no_sus.height)
			);		
	}
	inline bool getIsNovice(){return _is_novice;	}
	inline int getSuspensionCount(){return _novice_status_count;}
	inline double getHistMatchScore(){return hist_match_score;}
	inline Rect getResult(){return _result_temp;}
	inline Rect getBodysizeResult(){return _result_bodysize_temp;}	

	inline void updateKfCov(double body_width)
	{
		Mat m_temp=*(Mat_<float>(4,4)<<0.025,0,0,0,0,0.025,0,0,0,0,0.25,0,0,0,0,0.25);
		_kf.processNoiseCov=m_temp*AUX_kf_process*((float)body_width/_FRAME_RATE)*((float)body_width/_FRAME_RATE); // TODO: should update cov each step
		setIdentity(_kf.measurementNoiseCov,Scalar::all(1.0*AUX_kf_measure*(float)body_width*(float)body_width));
	}

private:
	inline void init_kf(Rect win)
	{
		_kf.statePost=*(Mat_<float>(4,1)<<win.x+0.5*win.width,win.y+0.5*win.height,0,0);
	}
	inline void correct_kf(KalmanFilter& kf, Rect win)
	{
		kf.correct(*(Mat_<float>(2,1)<<win.x+0.5*win.width,win.y+0.5*win.height));
	}
	inline double compareHisto(Mat& h)
	{
		return compareHist(hist,h,CV_COMP_INTERSECT);
	}

	typedef struct TraResult
	{
		Rect window;
		double likelihood;
		TraResult(Rect win,double like){window=win;likelihood=like;}
	}TraResult;

	size_t _refc;
	bool _is_dumped;
	static list<EnsembleTracker*> _TRASH_LIST;

	double _phi1_,_phi2_,_phi_max_;// system parameters

	int _ID;
	bool _is_novice;
	int _novice_status_count;//cout the consecutive times being a novice
	double _match_radius;

	list<AppTemplate*> _template_list;
	AppTemplate* _retained_template;//The last template when all others are eliminated
	int _template_count;//for its member
	vector<Rect> _result_history;
	vector<Rect> _filter_result_history;
	KalmanFilter _kf;
	
	int histSize[3];
	float _hRang[3][2];
	const float* hRange[3];
	int channels[3];
	MatND hist;// 3d histogram for stable appearance
	double hist_match_score;	

	Size2f _window_size;//
	Mat _confidence_map;
	Rect _cm_win;//roi for computing confidence map
	Rect _result_temp;//to store the meanshift result
	Rect _result_last_no_sus;// to store the last result when not _is_novice
	Rect _result_bodysize_temp;//GT size reuslt
	
	list<EnsembleTracker*> _neighbors;
	bool _added_new;
	Mat _recentHitRecord; 
	int _record_idx;
};


#endif