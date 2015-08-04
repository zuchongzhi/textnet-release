#ifndef TEXTNET_LAYER_MATCH_WEIGHTED_DOT_LAYER_INL_HPP_
#define TEXTNET_LAYER_MATCH_WEIGHTED_DOT_LAYER_INL_HPP_

#include <iostream>
#include <fstream>
#include <sstream>
#include <set>

#include <mshadow/tensor.h>
#include "../layer.h"
#include "../op.h"

namespace textnet {
namespace layer {

template<typename xpu>
class MatchWeightedDotLayer : public Layer<xpu>{
 public:
  MatchWeightedDotLayer(LayerType type) { this->layer_type = type; }
  virtual ~MatchWeightedDotLayer(void) {}
  
  virtual int BottomNodeNum() { return 2; }
  virtual int TopNodeNum() { return 1; }
  virtual int ParamNodeNum() { return 1; }
  
  virtual void Require() {
    // default value, just set the value you want
    this->defaults["interval"] = SettingV(1); 
    this->defaults["is_var_len"] = SettingV(true); 
    
    // require value, set to SettingV(),
    // it will force custom to set in config
    this->defaults["d_hidden"] = SettingV();

    this->defaults["w_filler"] = SettingV();
    this->defaults["w_updater"] = SettingV();
    
    Layer<xpu>::Require();
  }
  

  typedef mshadow::Tensor<xpu, 1> Tensor1D;
  typedef mshadow::Tensor<xpu, 2> Tensor2D;
  typedef mshadow::Tensor<xpu, 3> Tensor3D;
  typedef mshadow::Tensor<xpu, 4> Tensor4D;

  virtual void SetupLayer(std::map<std::string, SettingV> &setting,
                          const std::vector<Node<xpu>*> &bottom,
                          const std::vector<Node<xpu>*> &top,
                          mshadow::Random<xpu> *prnd) {
    Layer<xpu>::SetupLayer(setting, bottom, top, prnd);
    
    utils::Check(bottom.size() == BottomNodeNum(), "MatchWeightedDotLayer:bottom size problem."); 
    utils::Check(top.size() == TopNodeNum(), "MatchWeightedDotLayer:top size problem.");

    d_hidden = setting["d_hidden"].iVal();
    is_var_len = setting["is_var_len"].bVal();
    interval = setting["interval"].iVal();
	feat_size = bottom[0]->data.size(3);

    this->params.resize(1);
    this->params[0].Resize(d_hidden, feat_size, 1, 1, true); 

    std::map<std::string, SettingV> &w_setting = *setting["w_filler"].mVal();
    this->params[0].initializer_ = 
        initializer::CreateInitializer<xpu, 4>(w_setting["init_type"].iVal(), w_setting, this->prnd_);
    this->params[0].Init();

    std::map<std::string, SettingV> &w_updater = *setting["w_updater"].mVal();
    this->params[0].updater_ = updater::CreateUpdater<xpu, 4>(w_updater["updater_type"].iVal(),
                                                              w_updater, this->prnd_);
  }
  
  virtual void Reshape(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top,
					   bool show_info = false) {
    utils::Check(bottom.size() == BottomNodeNum(), "MatchWeightedDotLayer:bottom size problem."); 
    utils::Check(top.size() == TopNodeNum(), "MatchWeightedDotLayer:top size problem.");
                  
    batch_size = bottom[0]->data.size(0); 
    doc_len = bottom[0]->data.size(2);
                  
    // out_product.Resize(batch_size*doc_len*doc_len, feat_size, feat_size, 1, true);
    // top_data_swap.Resize(batch_size, doc_len, doc_len, d_hidden, true);
    top[0]->Resize(batch_size, d_hidden, doc_len, doc_len, true);

	if (show_info) {
		bottom[0]->PrintShape("bottom0");
		bottom[1]->PrintShape("bottom1");
		top[0]->PrintShape("top0");
	}
  }
  
  virtual void Forward(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
    Tensor4D bottom0_data = bottom[0]->data;
    Tensor4D bottom1_data = bottom[1]->data;
	Tensor1D bottom0_len  = bottom[0]->length_d1();
	Tensor1D bottom1_len  = bottom[1]->length_d1();
    Tensor4D top_data     = top[0]->data;
    Tensor2D w_data       = this->params[0].data_d2();

	top_data = 0.f;

    for (int batch_idx = 0; batch_idx < batch_size; ++batch_idx) {
      int len_0 = -1, len_1 = -1;
      if (is_var_len) {
        len_0 = bottom0_len[batch_idx];
        len_1 = bottom1_len[batch_idx];
      } else {
        len_0 = doc_len;
        len_1 = doc_len;
      }
      for (int i = 0; i < len_0; i+=interval) {
        Tensor1D rep_0 = bottom0_data[batch_idx][0][i];
        for (int j = 0; j < len_1; j+=interval) {
          Tensor1D rep_1 = bottom1_data[batch_idx][0][j];
          for (int h = 0; h < d_hidden; ++h) {
            for (int f = 0; f < feat_size; ++f) {
              top_data[batch_idx][h][i][j] += rep_0[f]*rep_1[f]*w_data[h][f];
            }
          }
        }
      }
    }
  }
  
  virtual void Backprop(const std::vector<Node<xpu>*> &bottom,
                        const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
    Tensor4D top_diff     = top[0]->diff;
    Tensor4D top_data     = top[0]->data;
	Tensor4D bottom0_data = bottom[0]->data;
	Tensor4D bottom1_data = bottom[1]->data;
	Tensor4D bottom0_diff = bottom[0]->diff;
	Tensor4D bottom1_diff = bottom[1]->diff;
	Tensor1D bottom0_len  = bottom[0]->length_d1();
	Tensor1D bottom1_len  = bottom[1]->length_d1();
	Tensor2D w_data       = this->params[0].data_d2();
	Tensor2D w_diff       = this->params[0].diff_d2();

    for (int batch_idx = 0; batch_idx < batch_size; ++batch_idx) {
      int len_0 = -1, len_1 = -1;
      if (is_var_len) {
        len_0 = bottom0_len[batch_idx];
        len_1 = bottom1_len[batch_idx];
      } else {
        len_0 = doc_len;
        len_1 = doc_len;
      }
      for (int i = 0; i < len_0; i+=interval) {
        Tensor1D rep_data_0 = bottom0_data[batch_idx][0][i];
        Tensor1D rep_diff_0 = bottom0_diff[batch_idx][0][i];
        for (int j = 0; j < len_1; j+=interval) {
          Tensor1D rep_data_1 = bottom1_data[batch_idx][0][j];
          Tensor1D rep_diff_1 = bottom1_diff[batch_idx][0][j];
          for (int h = 0; h < d_hidden; ++h) {
            for (int f = 0; f < feat_size; ++f) {
              rep_diff_0[f] += top_diff[batch_idx][h][i][j]*rep_data_1[f]*w_data[h][f];
              rep_diff_1[f] += top_diff[batch_idx][h][i][j]*rep_data_0[f]*w_data[h][f];
              w_diff[h][f]  += top_diff[batch_idx][h][i][j]*rep_data_0[f]*rep_data_1[f];
            }
          }
        }
      }
    }
  }
  
 protected:
  int doc_len, feat_size, batch_size, interval, d_hidden;
  bool is_var_len;
  // Node<xpu> out_product, top_data_swap;
};
}  // namespace layer
}  // namespace textnet
#endif 

