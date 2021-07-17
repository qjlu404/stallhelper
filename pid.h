#pragma once
class PIDImpl;
class PID
{
public:
    // Kp -  proportional gain
    // Ki -  Integral gain
    // Kd -  derivative gain
    // dt -  loop interval time
    // max - maximum value of manipulated variable
    // min - minimum value of manipulated variable
    PID(double Kp, double Ki, double Kd, double dt, double max, double min);

    // Returns the manipulated variable given a setpoint and current process value
    double calculate(double setpoint, double pv);
    ~PID();

private:
    PIDImpl* pimpl;
};

