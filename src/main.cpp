
#include <fstream>
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "json.hpp"
#include "spline.h"
#include <chrono>
#include <ctime>
typedef std::chrono::high_resolution_clock Clock;
//std::chrono::system_clock::time_point
//using namespace std::chrono; 
//using namespace std::chrono::system_clock;
using namespace std;

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

//Flag to capture if similator was re-started..
bool simulator_reset;

//Anshoo Add Struct
// Custom Structure to store data from Sensor
// Idea is to analyze, refine data query once every 20ms for better efficiency..
struct car_spec
  {
    	double car_id;
    	double car_lane_id;
    	double car_speed;
    	double car_distance_from_ego;
    	string car_orientation;
    	bool anyCarsInLaneUnderThershold = false;
    	double lane_avg_speed;

    	car_spec (double car_id, double car_lane_id, double car_speed, double car_distance_from_ego, string car_orientation, bool anyCarsInLaneUnderThershold, double lane_avg_speed) 
    				: car_id(car_id), car_lane_id(car_lane_id), car_speed(car_speed), car_distance_from_ego(car_distance_from_ego), car_orientation(car_orientation), anyCarsInLaneUnderThershold(anyCarsInLaneUnderThershold),lane_avg_speed(lane_avg_speed) {}
    	
    	car_spec() {}
  };

double MAX_VELOCITY = 49.5;
//End: Anshoo Add Struct

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

double distance(double x1, double y1, double x2, double y2)
{
	return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}

int ClosestWaypoint(double x, double y, const vector<double> &maps_x, const vector<double> &maps_y)
{

	double closestLen = 100000; //large number
	int closestWaypoint = 0;

	for(int i = 0; i < maps_x.size(); i++)
	{
		double map_x = maps_x[i];
		double map_y = maps_y[i];
		double dist = distance(x,y,map_x,map_y);
		if(dist < closestLen)
		{
			closestLen = dist;
			closestWaypoint = i;
		}

	}

	return closestWaypoint;

}

int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{

	int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

	double map_x = maps_x[closestWaypoint];
	double map_y = maps_y[closestWaypoint];

	double heading = atan2( (map_y-y),(map_x-x) );

	double angle = abs(theta-heading);

	if(angle > pi()/4)
	{
		closestWaypoint++;
	}

	return closestWaypoint;

}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

	int prev_wp;
	prev_wp = next_wp-1;
	if(next_wp == 0)
	{
		prev_wp  = maps_x.size()-1;
	}

	double n_x = maps_x[next_wp]-maps_x[prev_wp];
	double n_y = maps_y[next_wp]-maps_y[prev_wp];
	double x_x = x - maps_x[prev_wp];
	double x_y = y - maps_y[prev_wp];

	// find the projection of x onto n
	double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
	double proj_x = proj_norm*n_x;
	double proj_y = proj_norm*n_y;

	double frenet_d = distance(x_x,x_y,proj_x,proj_y);

	//see if d value is positive or negative by comparing it to a center point

	double center_x = 1000-maps_x[prev_wp];
	double center_y = 2000-maps_y[prev_wp];
	double centerToPos = distance(center_x,center_y,x_x,x_y);
	double centerToRef = distance(center_x,center_y,proj_x,proj_y);

	if(centerToPos <= centerToRef)
	{
		frenet_d *= -1;
	}

	// calculate s value
	double frenet_s = 0;
	for(int i = 0; i < prev_wp; i++)
	{
		frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
	}

	frenet_s += distance(0,0,proj_x,proj_y);

	return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<double> &maps_s, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int prev_wp = -1;

	while(s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1) ))
	{
		prev_wp++;
	}

	int wp2 = (prev_wp+1)%maps_x.size();

	double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),(maps_x[wp2]-maps_x[prev_wp]));
	// the x,y,s along the segment
	double seg_s = (s-maps_s[prev_wp]);

	double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
	double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

	double perp_heading = heading-pi()/2;

	double x = seg_x + d*cos(perp_heading);
	double y = seg_y + d*sin(perp_heading);

	return {x,y};

}

// Added by Anshoo
// Helper Method to give Lane Id with input as Frenet d value .. 
// Compute Lane (Left(0, Middle(1), Right(2))) based input car's d coorinate
int getCarCurrentLane (double d_val)
{
  int car_lane_id = -1;
  int lane_width = 4;

  if ((d_val > 0 ) && (d_val < lane_width ))
    car_lane_id = 0;
  else if ((d_val > lane_width ) && (d_val < 2*lane_width ))
    car_lane_id = 1;
  else if((d_val > 2*lane_width ) && (d_val < 3*lane_width ))
    car_lane_id = 2;
  else 
  	cout << " Car in just getting started or 4x4 Off-Roading Mode..!!" << endl;

  return car_lane_id;
}

//Added by Anshoo
// Helper Method returning closest promixity car's out of all given by sensor data
// This collection get's revisited every 20ms, however act as cache for 20ms helping with optimization of code..
vector<car_spec> getClosestCarsFromSensor(vector<vector<double>> sensor_data, double ego_s, int prev_size, int egoLane) {

    auto left_lane_distance = 9999;
    auto middle_lane_distance = 9999;
    auto right_lane_distance = 9999;

    auto left_cars_count = 0;
    auto middle_cars_count = 0;
    auto right_cars_count = 0;

    auto left_lane_avg_speed = 0.0;
    auto middle_lane_avg_speed = 0.0;
    auto right_lane_avg_speed = 0.0;

    vector<car_spec> closestCarReadings(3);
	
	for ( int i=0; i<sensor_data.size(); i++)
  	{
		double car_id = sensor_data[i][0]; // 0 is id
		double vx = sensor_data[i][3]; // 3 is vx
        double vy = sensor_data[i][4]; // 4 is vy
        double check_speed = sqrt(vx*vx+vy*vy); // calculating magnitude
        double check_car_s = sensor_data[i][5]; // 5 is s value

        // get future points for car in lane
        // as if we are using previous points, we may not be there yet
        // and we would have projection to where this car be in future ..
        check_car_s += ((double) prev_size*.02*check_speed);
        // Index 0: car lane id, Index 1: car lane in text 
        auto car_lane_id = getCarCurrentLane(sensor_data[i][6]); // 6 is d value
        auto car_distance_from_ego = check_car_s-ego_s;
        
        auto car_orientation = "";

        // Compute if car is ahead, behind or at same distance of ego.. 
        if (car_distance_from_ego == 0) {
        	car_orientation = "same";	
        } else if (car_distance_from_ego > 0) {
        	car_orientation = "ahead";	
        } else if (car_distance_from_ego < 0) {
        	car_orientation = "behind";	
        }

        car_distance_from_ego = abs(check_car_s-ego_s);

        // Populate Collection with refined closest distance cars
        // In addition compute average lane speed.., it is computed real-time to avoid another run/coputational cycles of collection..
		if (car_lane_id == 0 && car_distance_from_ego < left_lane_distance ){
        	left_cars_count += 1;
        	left_lane_avg_speed = (left_lane_avg_speed + check_speed)/left_cars_count;
        	closestCarReadings[0] = car_spec(car_id, car_lane_id, check_speed, car_distance_from_ego, car_orientation, false, left_lane_avg_speed);
        	left_lane_distance = car_distance_from_ego;
        } else if (car_lane_id == 1 && car_distance_from_ego < middle_lane_distance){
        	middle_cars_count += 1;
        	middle_lane_avg_speed = (middle_lane_avg_speed + check_speed)/middle_cars_count;
        	closestCarReadings[1] = car_spec(car_id, car_lane_id, check_speed, car_distance_from_ego, car_orientation, false, middle_lane_avg_speed);
        	middle_lane_distance = car_distance_from_ego;
        }else if (car_lane_id == 2 && car_distance_from_ego < right_lane_distance){
        	right_cars_count += 1;
        	right_lane_avg_speed = (right_lane_avg_speed + check_speed)/right_cars_count;
        	closestCarReadings[2] = car_spec(car_id, car_lane_id, check_speed, car_distance_from_ego, car_orientation, false, right_lane_avg_speed);
        	right_lane_distance = car_distance_from_ego;
        }
  	}

  	//cout << "Debug 6 : Cars Count Left, Middle, Right : >> " <<  left_cars_count << " -- " <<  middle_cars_count << " -- " <<  right_cars_count << endl;

  	return closestCarReadings;
}

// Added by Anshoo
// Method preparing what are viable actions if anamoly is identified.
// In this particular project, key anamoly is car ahead of us slowing down, making us
// 1. Either slown down OR 2. Change Lane
// Ideally we should have seperate action method however to keep things less complex
// prepare will execute the identified actions .. 
void prepareForAnamoly(vector<car_spec> closestCarReadings, double lane_change_distance_thershold, 
									int &egoLane, bool &tooClose, double &avg_lane_speed) {
	
	int laneChangeTo = egoLane;
	bool speedChangeRequired;
	double lane_speed;
	
	/** Debugging Code
	for (int i=0; i<3; i++){
		cout << "Closest Car Data:" << "Car Lane ID:" << closestCarReadings[i].car_lane_id << 
				 "--car_distance:" << closestCarReadings[i].car_distance_from_ego << " -- car_orientation:" << 
				"--anyCarsInLaneUnderThershold:" << closestCarReadings[i].anyCarsInLaneUnderThershold << " -lane_avg_speed:" <<
				closestCarReadings[i].lane_avg_speed << endl;
	}
	*/
				// Check if Lane Change a viable option ..
				// & If Ego on Leftmost lane, check for middle lane feasibility
				if (egoLane == 0 && abs(closestCarReadings[1].car_distance_from_ego) >= lane_change_distance_thershold){
					laneChangeTo = 1;
					lane_speed = closestCarReadings[1].lane_avg_speed;
					cout << "Lane change performed to Lane : " << laneChangeTo << endl;

					//cout << "debug 3 " << closestCarReadings[1].car_distance_from_ego << " -- "  <<
					//    (abs(closestCarReadings[1].car_distance_from_ego) >= lane_change_distance_thershold) << endl;
				} // If Ego on Middle lane, check for left lane feasibility as first priority
				else if (egoLane == 1 && abs(closestCarReadings[0].car_distance_from_ego) >= lane_change_distance_thershold){
					laneChangeTo = 0;
					lane_speed = closestCarReadings[0].lane_avg_speed;
					cout << "Lane change performed to Lane : " << laneChangeTo << endl;

					//cout << "debug 4 " << closestCarReadings[0].car_distance_from_ego << " -- "  <<
					  //  (abs(closestCarReadings[0].car_distance_from_ego) >= lane_change_distance_thershold) << endl;
				} // If Ego on Middle lane, check for right lane feasibility as second priority
				else if (egoLane == 1 && abs(closestCarReadings[2].car_distance_from_ego) >= lane_change_distance_thershold){
					laneChangeTo = 2;
					lane_speed = closestCarReadings[2].lane_avg_speed;
					cout << "Lane change performed to Lane : " << laneChangeTo << endl;

					//cout << "debug 5 " << closestCarReadings[2].car_distance_from_ego << " -- "  <<
					  //  (abs(closestCarReadings[2].car_distance_from_ego) >= lane_change_distance_thershold) << endl;
				} // If Ego on Rihtmost lane, check for left lane feasibility 
				else if (egoLane == 2 && abs(closestCarReadings[1].car_distance_from_ego) >= lane_change_distance_thershold){
					laneChangeTo = 1;
					lane_speed = closestCarReadings[1].lane_avg_speed;
					cout << "Lane change performed to Lane : " << laneChangeTo << endl;

					//cout << "debug 6 " << closestCarReadings[1].car_distance_from_ego << " -- "  <<
					 //  (abs(closestCarReadings[1].car_distance_from_ego) >= lane_change_distance_thershold) << endl;
				}
		
	

	if (laneChangeTo == egoLane) { // i.e Lane Change not feasible, reduce speed..
		speedChangeRequired = true;
		lane_speed = closestCarReadings[egoLane].lane_avg_speed;
		cout << "Speed must be reduced to avoid collision... " << endl;
	} // Penalize Lane Aggresive Lane Changes .. 

	//Execute Action by Updating reference values.. This can be seperate method wit eloborate controls..
	tooClose = speedChangeRequired;
	egoLane  = laneChangeTo;
	avg_lane_speed = lane_speed;
}

//Added by Anshoo
/**Ideation to have Planner with Cost Function
  - checkAnamoly()  
    - anyCarsUnderThershold() -- Like Car in Lane ahead and Slowing Down ..
  		- Find Car's in Promixity (Front, Left, Right --- Ahead/Behind with speed for all three)
  		- Available Lanes with Cost for Lane Change (if needed) 
  	- prepareForAnamoly
     	- suggestSlowDown
     	- OR suggestLaneChange
  	- executeActionForAnamoly
 **/
void checkForAnamoly(vector<vector<double>> sensor_data, double same_lane_distance_thershold, 
									double lane_change_distance_thershold, int &egoLane, double ego_s,
									int prev_size, bool &tooClose, double &avg_lane_speed, 
									double &forward_distance) {

	//Get Closest Cars Data
	vector<car_spec> closestCarReadings = getClosestCarsFromSensor(sensor_data, ego_s, prev_size, egoLane);

	//FIRST ANAMOLY USE-CASE: Check if any car in same lane & under thershold i.e  slowing down..
	//cout << "DEBUG 1 : Distance from Car Ahead in Lane : " << closestCarReadings[egoLane].car_distance_from_ego <<
	  //       " Heading : " << closestCarReadings[egoLane].car_orientation << endl;

	if (closestCarReadings[egoLane].car_orientation == "ahead") {
		forward_distance = closestCarReadings[egoLane].car_distance_from_ego;
	}

	if (closestCarReadings[egoLane].car_orientation == "ahead" &&
		closestCarReadings[egoLane].car_distance_from_ego > 0 && 
		closestCarReadings[egoLane].car_distance_from_ego < same_lane_distance_thershold){

		cout << "Anamoly Situation Trigerred, Prepare To Action in Progress... " << endl;
		// (slow or lane chnage) = prepareForAnamoly()
		// executeActionForAnamoly(slow or lane change)) .. 
		// Prepare itself is performing Action if needed, Action can seperate method ..
		prepareForAnamoly(closestCarReadings, lane_change_distance_thershold, egoLane, 
			              tooClose, avg_lane_speed);
	}
}

// Added by Anshoo
// Analyze the sensor data & compute avergae speed per lane..
vector<double> averageLaneSpeed(vector<vector<double>> sensor_data,  int prev_size)
{
	double left_lane_speed = 0;
	double center_lane_speed = 0;
	double right_lane_speed = 0;

  	double left_cars_count = 0;
  	double center_cars_count = 0;
  	double right_cars_count = 0;


  	// each vector will 3 parameter, difference in distance, collision_ind[0,1] and buffer_ind
  	for ( int i=0; i<sensor_data.size(); i++)
  	{
    	double vx = sensor_data[i][3];
    	double vy = sensor_data[i][4];
    	double d = sensor_data[i][6];

    	double check_speed = sqrt(vx*vx + vy*vy);

    	int lane = getCarCurrentLane(d);

    	if ( lane ==  0 ) //left lane
    	{
    		left_lane_speed += check_speed;
    		left_cars_count += 1;
    	}
    	else if ( lane == 1 )  //middle lane
    	{
    		center_lane_speed += check_speed;
    		center_cars_count += 1;
    	}
    	else if ( lane == 2) // right lane
    	{
    		right_lane_speed += check_speed;
    		right_cars_count += 1;
    	}

  	}

	if ( left_cars_count > 0 )
    {
    	left_lane_speed = left_lane_speed / left_cars_count;
    }
    else
    {
    	left_lane_speed = MAX_VELOCITY;
    }

    if ( center_cars_count > 0 )
    {
    	center_lane_speed = center_lane_speed / center_cars_count;
    }
    else
    {
    	center_lane_speed = MAX_VELOCITY;
    }

    if ( right_cars_count > 0 )
    {
    	right_lane_speed = right_lane_speed / right_cars_count;
    }
    else
    {
    	right_lane_speed = MAX_VELOCITY;
    }

  return {left_lane_speed, center_lane_speed, right_lane_speed};
}

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file_.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
  	istringstream iss(line);
  	double x;
  	double y;
  	float s;
  	float d_x;
  	float d_y;
  	iss >> x;
  	iss >> y;
  	iss >> s;
  	iss >> d_x;
  	iss >> d_y;
  	map_waypoints_x.push_back(x);
  	map_waypoints_y.push_back(y);
  	map_waypoints_s.push_back(s);
  	map_waypoints_dx.push_back(d_x);
  	map_waypoints_dy.push_back(d_y);
  }

//Added by Anshoo

// Start in Lane 1:
int lane = 1; //Middle Lane, far left is lane 0

// Move a reference velocity to target
// In this project 50 MPH is limit, anything over is a violation
// hence, we are trying to be as close to target speed..
double ref_vel = 0.0;//mph

// Define thersholds, pretty critical for successful outcome..
double same_lane_distance_thershold = 30; // distance in meters
double lane_change_distance_thershold = 40; // distance in meters
double avg_lane_speed = 0.0;

bool too_close = false;
double avg_speed = 0;
double foward_distance;

std::chrono::high_resolution_clock::time_point last_lane_change = std::chrono::high_resolution_clock::now();;

// END: Added by Anshoo

  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,&map_waypoints_dx,&map_waypoints_dy,&lane,&ref_vel,same_lane_distance_thershold,lane_change_distance_thershold,&avg_lane_speed,&too_close,&avg_speed,&foward_distance,&last_lane_change](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    //auto sdata = string(data).substr(0, length);
    //cout << sdata << endl;

    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          //std::cout << "Inside Event Telemetry...";

        	// Main car's localization Data
          	double car_x = j[1]["x"];
          	double car_y = j[1]["y"];
          	double car_s = j[1]["s"];
          	double car_d = j[1]["d"];
          	double car_yaw = j[1]["yaw"];
          	double car_speed = j[1]["speed"];

          	// Previous path data given to the Planner
          	auto previous_path_x = j[1]["previous_path_x"];
          	auto previous_path_y = j[1]["previous_path_y"];
          	
            // Previous path's end s and d values 
          	double end_path_s = j[1]["end_path_s"];
          	double end_path_d = j[1]["end_path_d"];

          	// Sensor Fusion Data, a list of all other cars on the same side of the road.
          	auto sensor_fusion = j[1]["sensor_fusion"];

          //Added by Anshoo

          	// if simulator is stopped and started again, reset the velocity
          	if (simulator_reset)
          	{
          		ref_vel = 1;
          		simulator_reset = false;
          	}

          	// Reset foward distance to high value every time & too_close be false (ie. 20ms in this case)
          	// and have this value be set by Anamoly logic.
          	// This value will cause aggressive breaking if below 5m 
          	foward_distance = 100;
          	too_close = false;
            // Having list of previous path size help us define
            // smooth transition to new path ..example 50 points.. 
            int prev_size = previous_path_x.size();

            //Sensor Fusion Logic
            // We are going to do this in Frenet as it is easier..
            if (prev_size>0) {
              car_s = end_path_s;
            }

            //Get current lane of the Ego Car .. 
            lane = getCarCurrentLane(car_d);
            int prev_ego_lane = lane;
            cout << "EGO IS DRIVING IN LANE " << lane << endl; 
            
            //Baseed on Sensor data, check if any Anamoly like slowing cars in front,
            // If so, prepare what actions may be viable and further execute them.. 
            checkForAnamoly(sensor_fusion, same_lane_distance_thershold, 
							lane_change_distance_thershold, lane, car_s,
						    prev_size, too_close, avg_lane_speed, 
						    foward_distance);

            //Speed/Velocity Control
            // If Ego Car is heading too close to car ahead in the same lane, slow down by factor
            // to avoid jerk.. 
            // Make use of lane average speed data to control de-acceleration sensitivity ..  
            vector<double> lanes_avg_speed = averageLaneSpeed(sensor_fusion, prev_size);

            /**
            // Avoid aggresive lane changes .. 
            auto present_time = std::chrono::high_resolution_clock::now();
			double time_diference_between_lane_changes = std::chrono::duration<double>(present_time - last_lane_change).count();

			cout << "Time Difference between Lane Changes in sec: " << time_diference_between_lane_changes << endl;

			if ( time_diference_between_lane_changes < 2 ) {
				cout << "Lane Change Penalized as too aggresive to lane change .." << endl;
				lane = prev_ego_lane;
			} 
			else { 
				cout << "Change Lane Executed .." << endl;
				prev_ego_lane = lane;
				// reset last lane change time .. 
				last_lane_change = std::chrono::high_resolution_clock::now();
			}
			*/

            cout << "Ego New Expected Lane: " << lane << endl;
            cout << "Ego Present Speed: " << ref_vel << endl;
            cout << "Ego Distance from car ahead: " << foward_distance << endl;
            cout << "Too Close Alert: " << too_close <<endl;
            cout << "Average Lane Speed" << lanes_avg_speed[lane] << endl;

            if (too_close)
            {
            	//Aggressive breaking to have overall speed come below 30 mph, i.e. 
            	//if distance is too short like 5m, else we would want to have min speed of 30 mph
            	
            	if (foward_distance <= 20 && ref_vel > 25){
            		ref_vel -= .224;
            		cout << "Ego new Reduced Speed Adjusted by Aggresive Breaking with factor .224 : " << ref_vel << endl;
            	}// Else maintain minimum speed and speed reduction by factor of average speed..
				else if ( ref_vel > lanes_avg_speed[lane] && ref_vel > 30) {
              		cout << "Ego new Reduced Speed: " << (ref_vel - (ref_vel/lanes_avg_speed[lane])/100 + .050) 
              		     << " -- By Applying Factor : " << ((ref_vel/lanes_avg_speed[lane])/100 + .050) << endl;

              		ref_vel -= (ref_vel/lanes_avg_speed[lane])/100 + .050; // Reduce Speed by Factor Plus Bias
              	}

            }
            else if ( ref_vel < 49.5 ){

              if ( ref_vel >= 0 and ref_vel <= 40)
              {
              	cout << "Ego new Increased Speed: " << (ref_vel + .224) 
              		     << " -- By Applying Factor : .224 " << endl;
                // to avoid max jerk warning when the simulator starts
                ref_vel += .224 ;
              }
              else
              {
              	cout << "Ego new Increased Speed: " << (ref_vel + (ref_vel/lanes_avg_speed[lane])/100) 
              		     << " -- By Applying Factor : " << ((ref_vel/lanes_avg_speed[lane])/100) << endl;

                ref_vel += (ref_vel/lanes_avg_speed[lane])/100; // Above 40mph Increase Speed by Factor Plus Bias
              }

              // avoid max speed exceeded
              if ( ref_vel > MAX_VELOCITY )
              {
              	cout << "Ego max speed adjustement applied to: " << MAX_VELOCITY << endl;
                ref_vel = MAX_VELOCITY;
              }

            }
            
          // End Sensor Fusion Logic

            // Sparsely placed waypoints placeholders
            // Create a list of widely spaced (x,y) waypoints,
            // evenly spaced at 30m. Later we will interoplate
            // these waypoints with spline & fill it in with more 
            // points that control space...
            vector<double> ptsx;
            vector<double> ptsy;

            //Keeping track of reference state x, y, yaw states
            // either we will reference the starting point or where the car
            // is or at the previous path end points..
            double ref_x = car_x;
            double ref_y = car_y;
            double ref_yaw = deg2rad(car_yaw);  

            // if previous path size is almost empty, use the car as starting reference
            if (prev_size < 2){
              //std::cout << "Inside Previous Path Size Almost Empty..." << endl;
              //Use two points that make the path tangent to the car
              double prev_car_x = car_x - cos(car_yaw);
              double prev_car_y = car_y - sin(car_yaw);

              ptsx.push_back(prev_car_x);
              ptsx.push_back(car_x);

              ptsy.push_back(prev_car_y);
              ptsy.push_back(car_y);
            } 
            //use the previous path's end point  as starting reference
            else {
              //std::cout << "Inside Previous Path Size NOT Empty..." << endl;
              //Redefine reference state as previous path end point
              ref_x = previous_path_x[prev_size-1];
              ref_y = previous_path_y[prev_size-1];

              double ref_x_prev = previous_path_x[prev_size-2];
              double ref_y_prev = previous_path_y[prev_size-2];
              ref_yaw = atan2(ref_y-ref_y_prev, ref_x-ref_x_prev);

              //Use the two points that make the path tangent to the
              // previous path's end point..
              ptsx.push_back(ref_x_prev);
              ptsx.push_back(ref_x);

              ptsy.push_back(ref_y_prev);
              ptsy.push_back(ref_y);

            }

            //cout << " Lane ID Just Prior Way Points  " << lane << endl;
            // In Frenet add evenly 30m spaced points ahead of the starting reference
            std::vector<double> next_wp0 = getXY(car_s+30, double(2+4 * lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            std::vector<double> next_wp1 = getXY(car_s+60, double(2+4 * lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            std::vector<double> next_wp2 = getXY(car_s+90, double(2+4 * lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            
            ptsx.push_back(next_wp0[0]);
            ptsx.push_back(next_wp1[0]);
            ptsx.push_back(next_wp2[0]);

            ptsy.push_back(next_wp0[1]);
            ptsy.push_back(next_wp1[1]);
            ptsy.push_back(next_wp2[1]);

            // We do shift car reference angle to 0 degrees
            // This is to see from cars perspective as it will make 
            // math much easier .. 
            for (int i=0; i<ptsx.size(); i++){

              double shift_x = ptsx[i]-ref_x;
              double shift_y = ptsy[i]-ref_y;

              ptsx[i] = (shift_x * cos(0-ref_yaw) - shift_y*sin(0-ref_yaw));
              ptsy[i] = (shift_x * sin(0-ref_yaw) + shift_y*cos(0-ref_yaw));

            }

            // create a spline
            tk::spline s;

            //set (x,y) points to the spline
            s.set_points(ptsx, ptsy);

            //define the actual (x,y) points we will use for the planner
            vector<double> next_x_vals;
            vector<double> next_y_vals;

            // This code is actually using the previous_path points
            // if any, as mentioned above this would help with 
            // smooth tranistion than starting from scratch again ..   
            for (int i=0; i<previous_path_x.size(); i++){
              //std::cout << "Inside Populating X & V Vals Vectors..." << endl;
              next_x_vals.push_back(previous_path_x[i]);
              next_y_vals.push_back(previous_path_y[i]);
            }

            // Calculate how to break up spline points so that we 
            // travel at our..
            // So above Spline call at with anchor X & Y would extropolate
            // but we want to spread these points with distances which gives us
            // desired speed. This requires some math :
            // Target = N * .02 * Velocity
            // 30m (any value we set as target) = N (Spread ex .5meters) * .02 (Time at which we revisit waypoints) * Velocity of Car
            // Below for loop shows he math ..
            double target_x = 30.0; // Set target to 30m
            double target_y = s(target_x); // Ask Spline to give us y points
            double target_dist = sqrt((target_x * target_x) + (target_y * target_y));

            double x_add_on=0;

            // Fill up the rest of the path with any points from previous path
            // This may be confusing, as we may feel that 50 minus
            // previous_path which is also 50 will be zero .. but 
            // in reality previous_path contains points which were 
            // not used by car yet, so say if car used only 3 points 
            // we have remaining 47, so this loop will add only 3 points to the waypoints..
            for (int i=1; i<= 20-previous_path_x.size(); i++ ){
              //std::cout << "Inside Populating Future Points..." << endl;
              double N = (target_dist/(.02*ref_vel/2.24)); //mph to meters/sec factor
              double x_point = x_add_on+(target_x/N); //X with Spread ..
              double y_point = s(x_point); // Ask Spline to gove Y according to Y

              x_add_on = x_point;

              double x_ref = x_point;
              double y_ref = y_point;

              //rotate back to normal coordinates after rotating it earlier
              x_point = (x_ref * cos(ref_yaw) - y_ref*sin(ref_yaw));
              y_point = (x_ref * sin(ref_yaw) + y_ref*cos(ref_yaw));

              x_point += ref_x;
              y_point += ref_y;

              next_x_vals.push_back(x_point);
              next_y_vals.push_back(y_point);
              
            } 
            //END: Added by Anshoo

          	json msgJson;

          	// TODO: define a path made up of (x,y) points that the car will visit sequentially every .02 seconds
          	// Original Lab Code, for begining purposes, start with code below to understand how waypoints help move car..
          	/** 
              double dist_inc = 0.5;
              for(int i = 0; i < 50; i++)
              {
                    //Precomputing precise coordinates for our car to stay in lane
                    // We can make use of Frenet Coordinates, 

                    // For S (foward) Coordinate, ideally we know 
                    // where car is, assuming it is in lane, we will take present
                    // s  coordinate, add next way point i+1 and add
                    // distance to increment ..
                    double next_s = car_s + (i+1) + dist_inc;

                    // For D (lateral) coordinate, we know that starting point is 
                    // middle lane, and each lane is 4 meter wide and that way points
                    // are measured from double yellow line (middle of road act as seperater)
                    // So we are 1.5 lane away from yellow lanes i.e where the way points are.
                    // So 1.5 times 4 meter is 6 
                    double next_d = 6;

                    // Now we know next_s & next_d, we are ready to define XY Vector,
                    // essentally converting Frenet to Global Map system..
                    vector<double> xy = getXY(next_s, next_d, map_waypoints_s, map_waypoints_x, map_waypoints_y); 

                    // Coordinates being pushed to NEXT X & Y coordinates, 
                    // This helps car move to the defined coordinates..
                    // We are generating next 50 Waypoints.. If we do not
                    // precompute and simpy use below line of commented code, this essentianlly
                    // mean that car will  have forward movement at present yaw
                    // essentially means that it will go straight (& off track!!)
                       //next_x_vals.push_back(car_x+(dist_inc*i)*cos(deg2rad(car_yaw)));
                       //next_y_vals.push_back(car_y+(dist_inc*i)*sin(deg2rad(car_yaw)));
                    next_x_vals.push_back(xy[0]);
                    next_y_vals.push_back(xy[1]);
              }     

            //END
            **/
            msgJson["next_x"] = next_x_vals;
          	msgJson["next_y"] = next_y_vals;

          	auto msg = "42[\"control\","+ msgJson.dump()+"]";

          	//this_thread::sleep_for(chrono::milliseconds(1000));
          	ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
          
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
    simulator_reset = true;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
