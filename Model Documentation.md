# CarND-Path-Planning-Project
Self-Driving Car Engineer Nanodegree Program
   

### Goals
In this project your goal is to safely navigate around a virtual highway with other traffic that is driving +-10 MPH of the 50 MPH speed limit.

Data & other relevant details, guiding principals provided by Udacity. 
1. Car's localization and sensor fusion data.
2. Sparse map list of waypoints around the highway.
3. Car should try to go as close as possible to the 50 MPH speed limit, which means passing slower traffic when possible, note that other cars will try to change lanes too.
4. The car should avoid hitting other cars at all cost as well as driving inside of the marked road lanes at all times, unless going from one lane to another.
5. The car should be able to make one complete loop around the 6946m highway. Since the car is trying to go 50 MPH, it should take a little over 5 minutes to complete 1 loop. 
6. Also the car should not experience total acceleration over 10 m/s^2 and jerk that is greater than 50 m/s^3.

### Reflection
This is thus far the best project, it was so much fun to work on this project, specifically initial phase when driving was anything but perfect, and insane amount of possibilities, at times creating quite funny situations.. 

I regret starting this project earlier, class labs were bit challenging, primarily becuase of my handicap with c++. And I got distracted to study/revise Term I, NN, CNN, Tensor concepts prior beginnig elective.. Nonetheless, back on track ..

Project walk-through video was great help, I do not think I had good connection of class labs with project asks. 

I started with template code & instructions from video, taking baby steps like:
1. Moving car by simlating waypoints & feeding them back to the unity engine. It was interesting to undersatnd concept of 50 waypoints, and how waypoints control speed etc aspect, very creative. Further, going in depth of defining starting 2+3 waypoints, and making use of SPLINE a magical utility simplifying this process to job for kids .. Spreading up of SPLINE points to N and understanding trignometry logic utilizing yaw rate computing reference points to be fed to Spline was quite a learning as well.
2. Trying to stay in lane by unnderstanding Frenet d value compution from double yellow line and each lane being 4 meter wide. walkthough gave very subtle hints on when to use Frenet for ease, optimization and later map them back to global coordinates. I spotted some irregularities with sensor data, perhaps it was latency causing them but this wasted great amount of time, debugging to see why car behavior is wierd & failing unpredictibally.  
3. Logic building to avoiding collision, understanding s coordinates and using them to see cars in each lane with distance from ego. 
4. And lastly, left only lane change as demostrated, but it was good enough to understand how to perform lane change. getXY() transform Frenet to Cartisian coordinates, we compute 3 future/ahead waypoint with dispacement factor of 30, 60, 90 and d, d value is factor added to the 2+4 which is nothing but middle of the lane, ie. if we switch lane from 0 (left) to 1 (right) -- cosidering from double yellow lines, middle of lane would be (2+4 * 1) = 6. And future waypoints would be calculated based on lane value or lane shoft value. These X & Y would be fed to Spline to derive 50 way points which basically help smoothen i.e. generate curve from present to target lane .. Havind said this, for some reason accuracy of my code was not upto mark specifcially lane change were not happening instant as expected. After debugging I thought  to reduce number of waypoints from 50 to 20. This gives us shorter prediction extrapolation path & better accuracy. This indeed work, though at one point we hit yellow line and warning flaged, though this happened post 15 miles of drive, and it could be excess CPU cycles / latency in play as well.. 

Post this I started conceptualizing basic rules on how I would drive/actions given circumstances - and started defining high level planner with cost function as stated below..

/**Ideation to have Planner with Cost Function
  - checkAnamoly()  
    - anyCarsUnderThershold() -- Like Car in Lane ahead and Slowing Down ..
      - Find Car's in Promixity (Front, Left, Right --- Ahead/Behind with
         speed for all three)
      - Available Lanes with Cost for Lane Change (if needed) 
    - prepareForAnamoly
      - suggestSlowDown
      - OR suggestLaneChange
    - executeActionForAnamoly
*/

This ideation pipeline was very helpful to start writing code. I basically started with helper functions ..
1. **getCarCurrentLane** : To give Lane Id with input as Frenet d value .. 
2. **getClosestCarsFromSensor** : Returning closest promixity car's out of all given by sensor data. This collection get's revisited every 20ms, however act as cache for 20ms helping with optimization of code..
3. **prepareForAnamoly** :Method preparing what are viable actions if anamoly is identified.In this particular project, key anamoly is car ahead of us slowing down, making us 1. Either slown down OR 2. Change Lane. Ideally we should have seperate action method however to keep things less complex
prepare will execute the identified actions ..
4. **checkForAnamoly** : This is sort of parent method following execution of above helper methods i.e. checking if Anamoly arise, if so, what action should be taken and further execute desired action .. 

**Hyper-parameters** - Though there were technically only 3 parameters to play with, it took me awefully long time to fine tune'em. The params used to have smooth operatios were..
1. Thershold distance indicating posible collision in same lane.
2. Thershold distance indiciating safe to make lane change.
3. Speed Progression to minimize jerk & effcetive lane changing..

**Speed Progression** is important topic to talk about, initially it worked but in some corner cases, because for sporadic speed adjustment, excessive breaking was applied causing collision & even at time out of lane, as speed is key factor generating waypoints via spline. The improvised approach I followed is that -
- Computed average lane speed for left, middle and right.
- Minimim speed of 30 mph is maintained along with maximum of 50 mph. Minimum speed helped keep all factors in control and very smooth run.
- If car ahead reach way closer than defined threshold (30m) like distance of 20m, aggressive breaking is applied to bring desired speed. This is the only scenario which bring speed down from 30 mph.
- Factor to reduce speed: unless aggressive, I used factor derived by Avg Lane Speed with Current Speed to compute smooth de-acceleration..
- For acceleration: from 0 - 40, used flat factor of .224. Post 40 mph, I used factor derived by Avg Lane Speed with Current Speed to compute smooth acceleration.. At higher speed aggressive acceleration and de-accelartion unless desired could lead to unpredictable behavior ..  

At this stage, and post many terrible failed attempts, I had successful run of 10+ miles.. (https://www.youtube.com/watch?v=pf4h2VsrJ5M&t=25s). 

**Areas to-be worked**
There are still few things which I think I can further look into, example -
1. Lane Average Speed to not have fluctuatiing speed unless there is clear way to speed up..
2. Seperately process, ahead and behind vehciles, taking absolute at  this moment, but hoping seperate accessment can help make swifter lane shits. I have coded orientation already, it is matter of implementing logic..
3. I see there are rare corner cases where car just adhoc lane change or group of cars jamm up the road. It was hard to reproduce these, so whatever best I could I did, but I think expanding lane from static 4 meter to something 4+/- thershold can help us give better control.

All in all, it was great learning and I would like to come back to this project to improvise it better .. 

Thank you .. 