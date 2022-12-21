
#include "esminiLib.hpp"

int main(int argc, char* argv[])
{
	SE_Init("../../resources/xosc/lane_change_variable.xosc", 0, 1, 0, 0);
	bool varTrigger = false;
	
	for (int i = 0; SE_GetQuitFlag() != 1; i++)
	{
		if (varTrigger == false && SE_GetSimulationTime() > 2.0f) 
		{
			varTrigger = true;
			SE_SetVariableDouble("DummyVariable", 15);
		}
		SE_Step();
	}

	SE_Close();

	return 0;
}
