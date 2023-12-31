#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <ros.h>

//header file for publishing velocities for odom
#include <prodigimover_msgs/Velocities.h>

//header file for cmd_subscribing to "cmd_vel"
#include <geometry_msgs/Twist.h>

//header file for pid server
#include <prodigimover_msgs/PID.h>

//header files for imu
#include <ros_arduino_msgs/RawImu.h>
#include <geometry_msgs/Vector3.h>

#include <ros/time.h>

#include <Wire.h>

#include "imu_configuration.h"
#include "prodigimover_base_config.h"
#include "Encoder.h"
#include "Motor.h"
#include "Kinematics.h"
#include "PID.h"

#define ENCODER_OPTIMIZE_INTERRUPTS

#define IMU_PUBLISH_RATE 10 //hz
#define VEL_PUBLISH_RATE 10 //hz
#define COMMAND_RATE 10 //hz
#define DEBUG_RATE 5

/*#ifdef L298_DRIVER
  //left side motors
  Motor motor1(MOTOR1_PWM, MOTOR1_IN_A, MOTOR1_IN_B); //front
  Motor motor3(MOTOR3_PWM, MOTOR3_IN_A, MOTOR3_IN_B);// rear
  //right side motors
  Motor motor2(MOTOR2_PWM, MOTOR2_IN_A, MOTOR2_IN_B); // front
  Motor motor4(MOTOR4_PWM, MOTOR4_IN_A, MOTOR4_IN_B); // rear
#endif*/

#ifdef BTS7960_DRIVER
  //left side motors
  Motor motor1(MOTOR1_IN_A, MOTOR1_IN_B); // front
  Motor motor3(MOTOR3_IN_A, MOTOR3_IN_B); // rear
  // right side motors
  Motor motor2(MOTOR2_IN_A, MOTOR2_IN_B); // front
  Motor motor4(MOTOR4_IN_A, MOTOR4_IN_B); // frear
#endif

//COUNTS_PER_REV = 0 if no encoder
int Motor::counts_per_rev_ = COUNTS_PER_REV;

//left side encoders
Encoder motor1_encoder(MOTOR1_ENCODER_A,MOTOR1_ENCODER_B); //front
Encoder motor3_encoder(MOTOR3_ENCODER_A,MOTOR3_ENCODER_B); //rear

//right side encoders
Encoder motor2_encoder(MOTOR2_ENCODER_A,MOTOR2_ENCODER_B); //front
Encoder motor4_encoder(MOTOR4_ENCODER_A,MOTOR4_ENCODER_B); //rear

Kinematics kinematics(MAX_RPM, WHEEL_DIAMETER, FR_WHEELS_DIST, LR_WHEELS_DIST, PWM_BITS);

PID motor1_pid(-255, 255, K_P, K_I, K_D);
PID motor2_pid(-255, 255, K_P, K_I, K_D);
PID motor3_pid(-255, 255, K_P, K_I, K_D);
PID motor4_pid(-255, 255, K_P, K_I, K_D);

double g_req_angular_vel_z = 0;
double g_req_linear_vel_x = 0;

unsigned long g_prev_command_time = 0;
unsigned long g_prev_control_time = 0;
unsigned long g_publish_vel_time = 0;
unsigned long g_prev_imu_time = 0;
unsigned long g_prev_debug_time = 0;

bool g_is_first = true;

char g_buffer[50];

//callback function prototypes
void commandCallback(const geometry_msgs::Twist& cmd_msg);
void PIDCallback(const prodigimover_msgs::PID& pid);

ros::NodeHandle nh;

ros::Subscriber<geometry_msgs::Twist> cmd_sub("cmd_vel", commandCallback);
ros::Subscriber<prodigimover_msgs::PID> pid_sub("pid", PIDCallback);

ros_arduino_msgs::RawImu raw_imu_msg;
ros::Publisher raw_imu_pub("raw_imu", &raw_imu_msg);

prodigimover_msgs::Velocities raw_vel_msg;
ros::Publisher raw_vel_pub("raw_vel", &raw_vel_msg);

void setup()
{
  nh.initNode();
  nh.getHardware()->setBaud(57600);
  nh.subscribe(pid_sub);
  nh.subscribe(cmd_sub);
  nh.advertise(raw_vel_pub);
  nh.advertise(raw_imu_pub);

  while (!nh.connected())
  {
    nh.spinOnce();
  }
  nh.loginfo("PRODIGIMOVER_BASE CONNECTED");

  Wire.begin();
  delay(5);
}

void loop()
{
  //this block drives the robot based on defined rate
  if ((millis() - g_prev_control_time) >= (1000 / COMMAND_RATE))
  {
    moveBase();
    g_prev_control_time = millis();
  }

  //this block stops the motor when no command is received
  if ((millis() - g_prev_command_time) >= 400)
  {
    stopBase();
  }

  //this block publishes velocity based on defined rate
  if ((millis() - g_publish_vel_time) >= (1000 / VEL_PUBLISH_RATE))
  {
    publishVelocities();
    g_publish_vel_time = millis();
  }

  //this block publishes the IMU data based on defined rate
  if ((millis() - g_prev_imu_time) >= (1000 / IMU_PUBLISH_RATE))
  {
    //sanity check if the IMU exits
    if (g_is_first)
    {
      checkIMU();
    }
    else
    {
      //publish the IMU data
      publishIMU();
    }
    g_prev_imu_time = millis();
  }

  //this block displays the encoder readings. change DEBUG to 0 if you don't want to display
  if(DEBUG)
  {
    if ((millis() - g_prev_debug_time) >= (1000 / DEBUG_RATE))
    {
      printDebug();
      g_prev_debug_time = millis();
    }
  }
  //call all the callbacks waiting to be called
  nh.spinOnce();
}

void PIDCallback(const prodigimover_msgs::PID& pid)
{
  //callback function every time PID constants are received from pid for tuning
  //this callback receives pid object where P,I, and D constants are stored
  motor1_pid.updateConstants(pid.p, pid.i, pid.d);
  motor2_pid.updateConstants(pid.p, pid.i, pid.d);
  motor3_pid.updateConstants(pid.p, pid.i, pid.d);
  motor4_pid.updateConstants(pid.p, pid.i, pid.d);
}

void commandCallback(const geometry_msgs::Twist& cmd_msg)
{
  //callback function every time linear and angular speed is received from 'cmd_vel' topic
  //this callback function receives cmd_msg object where linear and angular speed are stored
  g_req_linear_vel_x = cmd_msg.linear.x;
  g_req_angular_vel_z = cmd_msg.angular.z;

  g_prev_command_time = millis();
}

void moveBase()
{
  Kinematics::output req_rpm;
  //get the required rpm for each motor based on required velocities
  req_rpm = kinematics.getRPM(g_req_linear_vel_x, 0.0, g_req_angular_vel_z);

  //the required rpm is capped at -/+ MAX_RPM to prevent the PID from having too much error
  //the PWM value sent to the motor driver is the calculated PID based on required RPM vs measured RPM
  motor1.spin(motor1_pid.compute(constrain(req_rpm.motor1, -MAX_RPM, MAX_RPM), motor1.rpm));
  motor3.spin(motor3_pid.compute(constrain(req_rpm.motor3, -MAX_RPM, MAX_RPM), motor3.rpm));
  motor2.spin(motor2_pid.compute(constrain(req_rpm.motor2, -MAX_RPM, MAX_RPM), motor2.rpm));
  motor4.spin(motor4_pid.compute(constrain(req_rpm.motor4, -MAX_RPM, MAX_RPM), motor4.rpm));
}

void stopBase()
{
  g_req_linear_vel_x = 0.0;
  g_req_angular_vel_z = 0.0;
}

void publishVelocities()
{
  //update the current speed of each motor based on encoder's count
  motor1.updateSpeed(motor1_encoder.read());
  motor2.updateSpeed(motor2_encoder.read());
  motor3.updateSpeed(motor3_encoder.read());
  motor4.updateSpeed(motor4_encoder.read());

  Kinematics::velocities vel;
  vel = kinematics.getVelocities(motor1.rpm, motor2.rpm, motor3.rpm, motor4.rpm);

  //fill in the object
  raw_vel_msg.linear_x = vel.linear_x;
  raw_vel_msg.linear_y = 0.0;
  raw_vel_msg.angular_z = vel.angular_z;

  //publish raw_vel_msg object to ROS
  raw_vel_pub.publish(&raw_vel_msg);
}

void checkIMU()
{
  //this function checks if IMU is present
  raw_imu_msg.accelerometer = checkAccelerometer();
  raw_imu_msg.gyroscope = checkGyroscope();
  raw_imu_msg.magnetometer = checkMagnetometer();

  if (!raw_imu_msg.accelerometer)
  {
    nh.logerror("Accelerometer NOT FOUND!");
  }

  if (!raw_imu_msg.gyroscope)
  {
    nh.logerror("Gyroscope NOT FOUND!");
  }

  if (!raw_imu_msg.magnetometer)
  {
    nh.logerror("Magnetometer NOT FOUND!");
  }

  g_is_first = false;
}

void publishIMU()
{
  if (raw_imu_msg.accelerometer && raw_imu_msg.gyroscope && raw_imu_msg.magnetometer)
  {
    //this function publishes raw IMU reading
    raw_imu_msg.header.stamp = nh.now();
    raw_imu_msg.header.frame_id = "imu_link";

    //measure accelerometer
    if (raw_imu_msg.accelerometer)
    {
      measureAcceleration();
      raw_imu_msg.raw_linear_acceleration = raw_acceleration;
    }

    //measure gyroscope
    if (raw_imu_msg.gyroscope)
    {
      measureGyroscope();
      raw_imu_msg.raw_angular_velocity = raw_rotation;
    }

    //measure magnetometer
    if (raw_imu_msg.magnetometer)
    {
      measureMagnetometer();
      raw_imu_msg.raw_magnetic_field = raw_magnetic_field;
    }

    //publish raw_imu_msg object to ROS
    raw_imu_pub.publish(&raw_imu_msg);
  }
}

void printDebug()
{
  sprintf (g_buffer, "Encoder FrontLeft: %ld", motor1_encoder.read());
  nh.loginfo(g_buffer);
  sprintf (g_buffer, "Encoder RearLeft: %ld", motor3_encoder.read());
  nh.loginfo(g_buffer);
  sprintf (g_buffer, "Encoder FrontRight: %ld", motor2_encoder.read());
  nh.loginfo(g_buffer);
  sprintf (g_buffer, "Encoder RearRight: %ld", motor4_encoder.read());
  nh.loginfo(g_buffer);
}
