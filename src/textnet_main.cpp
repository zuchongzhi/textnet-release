#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE

#include <iostream>
#include <fstream>
#include <ctime>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <climits>
#include <sstream>

#include "./net/net.h"
#include "./layer/layer.h"
#include "./statistic/statistic.h"
#include "./io/json/json.h"
#include "global.h"

using namespace std;
using namespace textnet;
using namespace textnet::layer;
using namespace mshadow;
using namespace textnet::net;

void PrintTensor(const char * name, Tensor<cpu, 1> x) {
    Shape<1> s = x.shape_;
    cout << name << " shape " << s[0] << endl;
    for (unsigned int d1 = 0; d1 < s[0]; ++d1) {
      cout << x[d1] << " ";
    }
    cout << endl;
}

void PrintTensor(const char * name, Tensor<cpu, 2> x) {
    Shape<2> s = x.shape_;
    cout << name << " shape " << s[0] << "x" << s[1] << endl;
    for (unsigned int d1 = 0; d1 < s[0]; ++d1) {
      for (unsigned int d2 = 0; d2 < s[1]; ++d2) {
        cout << x[d1][d2] << " ";
      }
      cout << endl;
    }
    cout << endl;
}

void PrintTensor(const char * name, Tensor<cpu, 3> x) {
    Shape<3> s = x.shape_;
    cout << name << " shape " << s[0] << "x" << s[1] << "x" << s[2] << endl;
    for (unsigned int d1 = 0; d1 < s[0]; ++d1) {
        for (unsigned int d2 = 0; d2 < s[1]; ++d2) {
            for (unsigned int d3 = 0; d3 < s[2]; ++d3) {
                    cout << x[d1][d2][d3] << " ";
            }
            cout << ";";
        }
        cout << endl;
    }
}

void PrintTensor(const char * name, Tensor<cpu, 4> x) {
    Shape<4> s = x.shape_;
    cout << name << " shape " << s[0] << "x" << s[1] << "x" << s[2] << "x" << s[3] << endl;
    for (unsigned int d1 = 0; d1 < s[0]; ++d1) {
        for (unsigned int d2 = 0; d2 < s[1]; ++d2) {
            for (unsigned int d3 = 0; d3 < s[2]; ++d3) {
                for (unsigned int d4 = 0; d4 < s[3]; ++d4) {
                    cout << x[d1][d2][d3][d4] << " ";
                }
                cout << "|";
            }
            cout << ";";
        }
        cout << endl;
    }
    cout << endl;
}

int main(int argc, char *argv[]) {
  string model_file = "model/matching.tvt.model";
  DeviceType device_type = CPU_DEVICE;
  bool need_cross_valid = false;
  bool need_param = false;
  int netTagType = kTrainValidTest;

  if (argc > 1) {
    model_file = string(argv[1]);
  }
  for (int i = 2; i < argc; ++i) {
	if (string(argv[i]) == "cpu") {
		device_type = CPU_DEVICE;
	}
	if (string(argv[i]) == "gpu") {
		device_type = GPU_DEVICE;
	}
	if (string(argv[i]) == "-cv") {
		need_cross_valid = true;
	}
	if (string(argv[i]) == "-param") {
		need_param = true;
	}
	if (string(argv[i]) == "-nettype") {
		++i;
		if (string(argv[i]) == "Train") {
			netTagType = kTrainValidTest;
		} else if (string(argv[i]) == "Test") {
			netTagType = kTestOnly;
		}
	}
	
  }
  
  if ( !need_cross_valid ) {
    INet* net = CreateNet(device_type, netTagType);
	if ( !need_param ) {
		net->InitNet(model_file);
	} else {
		net->LoadModel(model_file);
	}

#if REALTIME_SERVER==1
    using namespace textnet::statistic;
    Statistic statistic;
    statistic.SetNet(net);
    statistic.Start();
#endif

    net->Start();
  } 
#if 0  
  else {
    int n_fold = cfg["cross_validation"].asInt();
    for (int i = 0; i < n_fold; ++i) {
      cout << "PROCESSING CROSS VALIDATION FOLD " << i << "..." << endl;
      stringstream ss;
      ss << i;
      Json::Value net_cfg = cfg;
      for (int layer_idx = 0; layer_idx < net_cfg["layers"].size(); ++layer_idx) {
        if (net_cfg["layers"][layer_idx]["setting"]["data_file"].isNull()) {
          continue;
        }
        string data_file = cfg["layers"][layer_idx]["setting"]["data_file"].asString();
        data_file += "."+ss.str();
        net_cfg["layers"][layer_idx]["setting"]["data_file"] = data_file;
      }

      INet* net = CreateNet(device_type, netTagType);
      net->InitNet(net_cfg);
      net->Start();
    }
  }

  ifs.close();
#endif 
  return 0;
}

