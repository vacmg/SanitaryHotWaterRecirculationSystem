//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_REMOTE_H
#define VALVE_SIDE_REMOTE_H

void handleCommsEvent();
void connectToHeater(bool ignoreErrors = false);
void serialEvent();

#endif //VALVE_SIDE_REMOTE_H