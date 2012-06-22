#ifndef MULTI_TRACK_ASSOCIATION
#define MULTI_TRACK_ASSOCIATION

#include <fstream>

#include "opencv2/opencv.hpp"

#include "parameter.h"
#include "dataReader.h"
#include "util.h"
#include "tracker.h"
#include "detector.h"

#define GOOD 0
#define NOTSURE 1
#define BAD 2

#define COUNT_NUM 1000.0
#define SLIDING_WIN_SIZE 7.2*TIME_WINDOW_SIZE 


using namespace cv;

// stores configuration for tracker
class HE_Config
{
public:
	HE_Config();
	void read(string filename);
private:
	int _frame_rate;
	int _time_window_size;
	
	int _tracker_max_number;
	int _template_max_number;
	int _threshold_for_expert;

	double _bodysize_detection_ratio;//0.64
	double _tracking_bodysize_ratio;//0.5
	double _tracking_detection_ratio;
	
	double _detector_frame_resize_ratio;

	double _hist_update_rate;
	double _novice_result_filter_hist_thresh;
};

class WaitingList
{
	typedef struct Waiting
	{
		int accu;
		Rect currentWin;
		Point center;
		int life_count;
		Waiting(Rect win)
			:accu(1),
			life_count(1),
			currentWin(win),
			center((int)(win.x+0.5*win.width),(int)(win.y+0.5*win.height))
		{
		}
	}Waiting;

	list<Waiting> w_list;
	int life_limit;

public:
	WaitingList(int life):life_limit(life){}
	void update();
	vector<Rect>outputQualified(double thresh);
	void feed(Rect bodysize_win,double response);
};

class Controller
{
public:
	WaitingList waitList;
	WaitingList waitList_suspicious;

	Controller(
		Size sz,int r, int c,double vh=0.01,
		double lr=1/COUNT_NUM,
		double thresh_expert=0.5);
	void takeVoteForHeight(Rect bodysize_win);	
	vector<int> filterDetection(vector<Rect> detction_bodysize);	
	void takeVoteForAvgHittingRate(list<EnsembleTracker*> _tracker_list);	
	void deleteObsoleteTracker(list<EnsembleTracker*>& _tracker_list);	
	void calcSuspiciousArea(list<EnsembleTracker*>& _tracker_list);	
	inline vector<Rect> getQualifiedCandidates()
	{
		double l=_hit_record._getAvgHittingRate(_alpha_hitting_rate,_beta_hitting_rate);
		return waitList.outputQualified((l-sqrt(l)*AUX_gamma1-1.0));		
	}
private:
	// a rotate array for keep record of the average hitting rate
	typedef struct HittingRecord
	{
		Mat record;
		int idx;
		HittingRecord():idx(0)
		{
			record=Mat::zeros(2,(int)(SLIDING_WIN_SIZE),CV_64FC1);
		}
		void recordVote(bool vote)
		{
			idx=idx-record.cols*(idx/record.cols);
			record.at<double>(0,idx)=vote ? 1.0:0;
			record.at<double>(1,idx)=1;
			idx++;
		}
		double _getAvgHittingRate(double _alpha_hitting_rate, double _beta_hitting_rate)
		{
			Scalar s1=sum(record.row(0));
			Scalar s2=sum(record.row(1));
			return (s1[0]*TIME_WINDOW_SIZE+_alpha_hitting_rate)/(_beta_hitting_rate+s2[0]);
		}
	}HittingRecord;

	double _thresh_for_expert;
	
	Size _frame_size;
	int _grid_rows;
	int _grid_cols;
	double _prior_height_variance;
	vector<vector<double>> _bodyheight_map;
	vector<vector<double>> _bodyheight_map_count;
	double _bodyheight_learning_rate;

	HittingRecord _hit_record;
	double _alpha_hitting_rate;
	double _beta_hitting_rate;

	vector<Rect> _suspicious_rect_list;
};

class TrakerManager
{
public:
	TrakerManager(
		Detector* detctor,Mat& frame,
		double thresh_promotion);
	~TrakerManager();	
	void doWork(Mat& frame);

	void setKey(char c)
	{
		_my_char = c;
	}	
private:	
	void doHungarianAlg(const vector<Rect>& detections);
	inline static bool compareTraGroup(EnsembleTracker* c1,EnsembleTracker* c2)
	{
		return c1->getTemplateNum()>c2->getTemplateNum() ? true:false;
	}

	Controller _controller;
	Mat* _frame_set;
	list<EnsembleTracker*> _tracker_list;
	int _tracker_count;
	char _my_char;		
	
	Detector* _detector;
	int _frame_count;
	
	Mat _occupancy_map;	
	XMLBBoxWriter resultWriter;

	//TEMP 
	double _expert_perf_count;
	double _tracker_perf_count;
	//TEMP

	double _thresh_for_expert_;
};
	


#endif