#include <aslam/backend/LevenbergMarquardtTrustRegionPolicy.hpp>
#include <aslam/backend/OptimizationProblem.hpp>
#include <aslam/backend/Optimizer2.hpp>
#include <aslam/backend/SparseCholeskyLinearSystemSolver.hpp>
#include <sm/BoostPropertyTree.hpp>

#include <aslam/calibration/calibrator/AbstractCalibrator.h>
#include "aslam/calibration/calibrator/CalibratorI.hpp"
#include <aslam/calibration/CalibrationProblem.hpp>
#include <aslam/calibration/data/MapStorage.h>
#include <aslam/calibration/model/StateCarrier.h>
#include <aslam/calibration/model/Sensor.hpp>

namespace aslam {
namespace calibration {

class BatchCalibratorOptions : public AbstractCalibratorOptions {
  using AbstractCalibratorOptions::AbstractCalibratorOptions;
};

class BatchEstConf : public EstConf {
 public:
  virtual ~BatchEstConf(){}

  const Activator& getCalibrationActivator() const override {
    return AllActiveActivator;
  }

  const Activator& getStateActivator() const override {
    return AllActiveActivator;
  }

  const Activator& getErrorTermActivator() const override {
    return AllActiveActivator;
  }

  bool isSpatialActive() const override {
    return true;
  }
  bool isTemporalActive() const override {
    return true;
  }

  std::string getOutputFolder(size_t /*segmentIndex*/ = 0) const override {
    return "output/";
  }

  bool getUseCalibPriors() const override {
    return useCalibPriors_;
  }
  void setUseCalibPriors(bool useCalibPriors = false) {
    useCalibPriors_ = useCalibPriors;
  }

  void print(std::ostream & o) const override {
    o << "BatchEstimationConfig()";
  }

 private:
  bool useCalibPriors_ = false;
};

class BatchCalibrationProblem : public CalibrationProblem, public BatchStateReceiver {
 public:
  BatchCalibrationProblem() :
    problemSp_(new backend::OptimizationProblem()),
    problem_(*problemSp_)
  {
  }

  virtual ~BatchCalibrationProblem() {}

  void addCalibrationVariable(CalibrationVariable* c) override {
    CHECK_NOTNULL(c);
    dimCalibVariables_ += c->getDesignVariable().minimalDimensions();
    problem_.addDesignVariable(aslam::backend::DesignVariable::Ptr(&c->getDesignVariable(), sm::null_deleter()));
  }

  void addStateVariable(backend::DesignVariable* s) override {
    CHECK_NOTNULL(s);
    dimStateVariables_ += s->minimalDimensions();
    problem_.addDesignVariable(aslam::backend::DesignVariable::Ptr(s, sm::null_deleter()));
  }

  size_t getDimCalibrationVariables() const override {
    return dimCalibVariables_;
  }
  size_t getDimStateVariables() const override {
    return dimStateVariables_;
  }
  size_t getNumErrorTerms() const override {
    return problem_.numErrorTerms();
  }

  void addErrorTerm(const boost::shared_ptr<aslam::backend::ErrorTerm> & et) override {
    problem_.addErrorTerm(et);
  }

  void addBatchState(StateCarrier & /*stateCarrier*/, const BatchStateSP& /*batchState*/) override {

  }

  virtual const std::vector<boost::shared_ptr<backend::ErrorTerm>> & getErrorTerms() const override {
    struct A : public backend::OptimizationProblem {
      const std::vector<boost::shared_ptr<backend::ErrorTerm>> & getErrorTerms() const {
        return _errorTerms;
      }
    };

    return static_cast<A&>(problem_).getErrorTerms();
  }

  virtual void getErrors(const backend::DesignVariable* dv, std::set<backend::ErrorTerm*>& outErrorSet) const override {
    problem_.getErrors(dv, outErrorSet);
  }

  const boost::shared_ptr<backend::OptimizationProblem>& getProblemSp() const {
    return problemSp_;
  }

 private:
  boost::shared_ptr<backend::OptimizationProblem> problemSp_;
  backend::OptimizationProblem & problem_;

  size_t dimCalibVariables_ = 0, dimStateVariables_ = 0;
};

class BatchCalibrator : public virtual BatchCalibratorI, public AbstractCalibrator {
 public:
  BatchCalibrator (ValueStoreRef config, std::shared_ptr<Model> model) :
    AbstractCalibrator(config, model),
    config_(config),
    options_(config)
  {
  }

  virtual void calibrate() override {
    LOG(INFO) << "Before calibration:" << std::endl << getModel() << std::endl;
    LOG(INFO) << "Staring calibration in interval " << secsSinceStart(getCurrentEffectiveBatchInterval());

    for(Module & m : getModel().getModules()){
      m.preProcessNewWindow(*this);
    }
    if(!initStates()){
      LOG(FATAL) << "initStates failed";
      return;
    }

    BatchEstConf estConf;
    BatchCalibrationProblem problem;

    estimate(estConf, problem, problem, [&](){
      boost::shared_ptr<backend::LinearSystemSolver> linearSystemSolver(new aslam::backend::SparseCholeskyLinearSystemSolver());
      linearSystemSolver->setAcceptConstantErrorTerms(options_.getAcceptConstantErrorTerms());

      aslam::backend::Optimizer2 opt(config_.getChild("estimator/optimizer").asPropertyTree(), linearSystemSolver, boost::make_shared<backend::LevenbergMarquardtTrustRegionPolicy>(100));
      updateOptimizerInspector(problem, false, [&](std::ostream &out){
          out << "The Jacobian matrix is: " << linearSystemSolver->JRows()<< " x " << linearSystemSolver->JCols();
      }, opt.callback());
      opt.setProblem(problem.getProblemSp());
      opt.options().verbose = false;
      LOG(INFO) << "Optimizer options for batch estimation:" << opt.getOptions();
      opt.optimize();
      LOG(INFO) << "Final "<< opt.getStatus();
    });

    getModel().printCalibrationVariables(LOG(INFO) << "After calibration:" << std::endl) << std::endl;
  }

  BatchCalibratorOptions& getOptions() {
    return options_;
  }
  const BatchCalibratorOptions& getOptions() const override {
    return options_;
  }

  bool handleNewTimeBaseTimestamp(Timestamp t) override {
    if (_currentEffectiveBatchInterval.start == InvalidTimestamp()){
      _currentEffectiveBatchInterval.start = t;
      _currentEffectiveBatchInterval.end = t;
      return true;
    } else {
      if(_currentEffectiveBatchInterval.start > t){
        _currentEffectiveBatchInterval.start = t;
        return true;
      }
      if(_currentEffectiveBatchInterval.end < t){
        _currentEffectiveBatchInterval.end = t;
        return true;
      }
      return false;
    }
  }

  virtual ModuleStorage & getCurrentStorage() override {
    return storage_;
  }

 private:
  sm::value_store::ValueStoreRef config_;
  BatchCalibratorOptions options_;
  MapStorage<const Module *> storage_;
};


std::unique_ptr<BatchCalibratorI> createBatchCalibrator(ValueStoreRef vs, std::shared_ptr<Model> model) {
  return std::unique_ptr<BatchCalibratorI>(new BatchCalibrator(vs, model));
}

}
}
