//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_ACTUATORS_H
#define VALVE_SIDE_ACTUATORS_H

void setPumpTimeout(long timeout);
void setValve(bool enable);
void setPump(bool enable, bool ignoreErrors = false);

#endif //VALVE_SIDE_ACTUATORS_H