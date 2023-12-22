#include "prodigimover_base.h"

int main(int argc, char** argv )
{
    ros::init(argc, argv, "prodigimover_base_node");
    ProdigiMoverBase prodigimover;
    ros::spin();
    return 0;
}
