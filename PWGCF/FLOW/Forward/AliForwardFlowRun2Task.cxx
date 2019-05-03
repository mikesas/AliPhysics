
//
// Calculate flow in the forward and central regions using the Q cumulants method.
//
// Inputs:
//  - AliAODEvent
//
// Outputs:
//  - AnalysisResults.root or forward_flow.root
//
#include <iostream>
#include <TROOT.h>
#include <TSystem.h>
#include <TInterpreter.h>
#include <TList.h>
#include <THn.h>

#include "AliLog.h"
#include "AliForwardFlowRun2Task.h"
#include "AliForwardQCumulantRun2.h"
#include "AliForwardGenericFramework.h"

#include "AliAODForwardMult.h"
#include "AliAODCentralMult.h"
#include "AliAODEvent.h"
#include "AliMCEvent.h"

#include "AliForwardFlowUtil.h"

#include "AliVVZERO.h"
#include "AliAODVertex.h"
#include "AliCentrality.h"

#include "AliESDEvent.h"
#include "AliVTrack.h"
#include "AliESDtrack.h"
#include "AliAODTrack.h"
#include "AliAODTracklets.h"

#include "AliAnalysisFilter.h"
#include "AliMultSelection.h"
#include "AliMultiplicity.h"
#include "AliAnalysisManager.h"
#include "AliInputEventHandler.h"
#include "AliAODMCParticle.h"
#include "AliStack.h"
#include "AliMCEvent.h"
#include "AliMCParticle.h"

using namespace std;
ClassImp(AliForwardFlowRun2Task)
#if 0
; // For emacs
#endif


//_____________________________________________________________________
AliForwardFlowRun2Task::AliForwardFlowRun2Task() : AliAnalysisTaskSE(),
  fAOD(0),           // input event
  fOutputList(0),    // output list
  fAnalysisList(0),
  fEventList(0),
  fRandom(0),
  centralDist(),
  refDist(),
  forwardDist(),
  fSettings(),
  fUtil(),
  useEvent(true)
  {
  //
  //  Default constructor
  //
  }

//_____________________________________________________________________
AliForwardFlowRun2Task::AliForwardFlowRun2Task(const char* name) : AliAnalysisTaskSE(name),
  fAOD(0),           // input event
  fOutputList(0),    // output list
  fAnalysisList(0),
  fEventList(0),
  fRandom(0),
  centralDist(),
  refDist(),
  forwardDist(),
  fSettings(),
  fUtil(),
  useEvent(true)
  {
  //
  //  Constructor
  //
  //  Parameters:
  //   name: Name of task
  //

  // Rely on validation task for event and track selection
  DefineInput(1, AliForwardTaskValidation::Class());
  // DefineInput(2, TList::Class());
  // DefineInput(3, TList::Class());
  // DefineInput(4, TList::Class());
  DefineOutput(1, TList::Class());
}

//_____________________________________________________________________
void AliForwardFlowRun2Task::UserCreateOutputObjects()
  {
    //
    //  Create output objects
    //
    //bool saveAutoAdd = TH1::AddDirectoryStatus();

    fOutputList = new TList();          // the final output list
    fOutputList->SetOwner(kTRUE);       // memory stuff: the list is owner of all objects it contains and will delete them if requested

    TRandom r = TRandom();              // random integer to use for creation of samples (used for error bars).
                                        // Needs to be created here, otherwise it will draw the same random number.

    fAnalysisList    = new TList();
    fEventList       = new TList();
    fAnalysisList   ->SetName("Analysis");
    fEventList      ->SetName("EventInfo");

    fEventList->Add(new TH1D("Centrality","Centrality",fSettings.fCentBins,0,100));
    fEventList->Add(new TH1D("Vertex","Vertex",fSettings.fNZvtxBins,fSettings.fZVtxAcceptanceLowEdge,fSettings.fZVtxAcceptanceUpEdge));
    //fEventList->Add(new TH1D("FMDHits","FMDHits",100,0,10));
    fEventList->Add(new TH2F("dNdeta","dNdeta",200 /*fSettings.fNDiffEtaBins*/,fSettings.fEtaLowEdge,fSettings.fEtaUpEdge,fSettings.fCentBins,0,100));

    fAnalysisList->Add(new TList());
    fAnalysisList->Add(new TList());
    fAnalysisList->Add(new TList());
    static_cast<TList*>(fAnalysisList->At(0))->SetName("Reference");
    static_cast<TList*>(fAnalysisList->At(1))->SetName("Differential");
    static_cast<TList*>(fAnalysisList->At(2))->SetName("AutoCorrection");

    fOutputList->Add(fAnalysisList);
    fOutputList->Add(fEventList);

    // do analysis from v_2 to a maximum of v_5
    Int_t fMaxMoment = 5;
    Int_t dimensions = 5;

    Int_t dbins[5] = {fSettings.fnoSamples, fSettings.fNZvtxBins, fSettings.fNDiffEtaBins, fSettings.fCentBins, static_cast<Int_t>(fSettings.kW4ThreeTwoB)+1} ;
    Int_t rbins[5] = {fSettings.fnoSamples, fSettings.fNZvtxBins, fSettings.fNRefEtaBins, fSettings.fCentBins, static_cast<Int_t>(fSettings.kW4ThreeTwoB)+1} ;
    Double_t xmin[5] = {0,fSettings.fZVtxAcceptanceLowEdge, fSettings.fEtaLowEdge, 0, 0};
    Double_t xmax[5] = {10,fSettings.fZVtxAcceptanceUpEdge, fSettings.fEtaUpEdge, 100, static_cast<Double_t>(fSettings.kW4ThreeTwoB)+1};

    static_cast<TList*>(fAnalysisList->At(2))->Add(new THnD("fQcorrfactor", "fQcorrfactor", dimensions, rbins, xmin, xmax)); //(eta, n)
    static_cast<TList*>(fAnalysisList->At(2))->Add(new THnD("fpcorrfactor","fpcorrfactor", dimensions, dbins, xmin, xmax)); //(eta, n)
    Int_t ptnmax =  (fSettings.doPt ? 10 : 0);

    // create a THn for each harmonic
    for (Int_t n = 2; n <= fMaxMoment; n++) {
      for (Int_t ptn = 0; ptn <= ptnmax; ptn++){

        static_cast<TList*>(fAnalysisList->At(0))->Add(new THnD(Form("cumuRef_v%d_pt%d", n,ptn), Form("cumuRef_v%d_pt%d", n,ptn), dimensions, rbins, xmin, xmax));
        static_cast<TList*>(fAnalysisList->At(1))->Add(new THnD(Form("cumuDiff_v%d_pt%d", n,ptn),Form("cumuDiff_v%d_pt%d", n,ptn), dimensions, dbins, xmin, xmax));
        // The THn has dimensions [random samples, vertex position, eta, centrality, kind of variable to store]
        // set names
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(0))   ->FindObject(Form("cumuRef_v%d_pt%d", n,ptn)))->GetAxis(0)->SetName("samples");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(0))   ->FindObject(Form("cumuRef_v%d_pt%d", n,ptn)))->GetAxis(1)->SetName("vertex");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(0))   ->FindObject(Form("cumuRef_v%d_pt%d", n,ptn)))->GetAxis(2)->SetName("eta");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(0))   ->FindObject(Form("cumuRef_v%d_pt%d", n,ptn)))->GetAxis(3)->SetName("cent");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(0))   ->FindObject(Form("cumuRef_v%d_pt%d", n,ptn)))->GetAxis(4)->SetName("identifier");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(1))   ->FindObject(Form("cumuDiff_v%d_pt%d", n,ptn)))->GetAxis(0)->SetName("samples");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(1))   ->FindObject(Form("cumuDiff_v%d_pt%d", n,ptn)))->GetAxis(1)->SetName("vertex");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(1))   ->FindObject(Form("cumuDiff_v%d_pt%d", n,ptn)))->GetAxis(2)->SetName("eta");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(1))   ->FindObject(Form("cumuDiff_v%d_pt%d", n,ptn)))->GetAxis(3)->SetName("cent");
        static_cast<THnD*>(static_cast<TList*>(fAnalysisList->At(1))   ->FindObject(Form("cumuDiff_v%d_pt%d", n,ptn)))->GetAxis(4)->SetName("identifier");
      }
    }
    std::cout << "doNUA = " << boolalpha << fSettings.doNUA << std::endl;
    std::cout << "sec_corr = " << boolalpha << fSettings.sec_corr << std::endl;


  PostData(1, fOutputList);
  //TH1::AddDirectory(saveAutoAdd);
}


//_____________________________________________________________________
void AliForwardFlowRun2Task::UserExec(Option_t *)
  {
  //
  //  Analyses the event with use of the helper class AliForwardQCumulantRun2
  //
  //  Parameters:
  //   option: Not used
  //

  // Get the event validation object
   AliForwardTaskValidation* ev_val = dynamic_cast<AliForwardTaskValidation*>(this->GetInputData(1));
   if (!ev_val->IsValidEvent()){
      PostData(1, this->fOutputList);
     return;
   }

  if (!fSettings.esd){
    AliAODEvent* aodevent = dynamic_cast<AliAODEvent*>(InputEvent());
    fUtil.fAODevent = aodevent;
    if(!aodevent) throw std::runtime_error("Not AOD as expected");
  }
  if (fSettings.mc) fUtil.fMCevent = this->MCEvent();

  fUtil.fevent = fInputEvent;
  fUtil.fSettings = fSettings;

  Double_t cent = fUtil.GetCentrality(fSettings.centrality_estimator);
  // if (cent > 60.0){
  //   PostData(1, fOutputList);
  //   return;
  // }

  // Make centralDist
  Int_t   centralEtaBins = (fSettings.useITS ? 200 : 400);
  Int_t   centralPhiBins = (fSettings.useITS ? 20 : 400);
  Double_t centralEtaMin = (fSettings.useSPD ? -2.5 : fSettings.useITS ? -4 : -1.5);
  Double_t centralEtaMax = (fSettings.useSPD ? 2.5 : fSettings.useITS ? 6 : 1.5);

  // Make refDist
  Int_t   refEtaBins = (((fSettings.ref_mode & fSettings.kITSref) | (fSettings.ref_mode & fSettings.kFMDref)) ? 200 : 400);
  Int_t   refPhiBins = (((fSettings.ref_mode & fSettings.kITSref) | (fSettings.ref_mode & fSettings.kFMDref)) ? 20  : 400);
  Double_t refEtaMin = ((fSettings.ref_mode & fSettings.kSPDref) ? -2.5 
                             : ((fSettings.ref_mode & fSettings.kITSref) | (fSettings.ref_mode & fSettings.kFMDref)) ? -4 
                             : -1.5);
  Double_t refEtaMax = ((fSettings.ref_mode & fSettings.kSPDref) ?  2.5 
                             : ((fSettings.ref_mode & fSettings.kITSref) | (fSettings.ref_mode & fSettings.kFMDref)) ? 6 
                             : 1.5);


  TH2D centralDist_tmp = TH2D("c","",centralEtaBins,centralEtaMin,centralEtaMax,centralPhiBins,0,2*TMath::Pi());
  centralDist_tmp.SetDirectory(0);

  TH2D refDist_tmp = TH2D("r","",refEtaBins,refEtaMin,refEtaMax,refPhiBins,0,2*TMath::Pi());
  refDist_tmp.SetDirectory(0);

  TH2D forwardDist_tmp = TH2D("ft","",200,-4,6,20,0,TMath::TwoPi());
  forwardDist_tmp.SetDirectory(0);

  centralDist = &centralDist_tmp;
  centralDist ->SetDirectory(0);
  refDist     = &refDist_tmp;
  refDist     ->SetDirectory(0);
  forwardDist = &forwardDist_tmp;
  forwardDist ->SetDirectory(0);

  fUtil.FillData(refDist,centralDist,forwardDist);

  // dNdeta
  TH2F* dNdeta = static_cast<TH2F*>(fEventList->FindObject("dNdeta"));
  dNdeta->SetDirectory(0);
  for (Int_t etaBin = 1; etaBin <= centralDist->GetNbinsX(); etaBin++) {
    Double_t eta = centralDist->GetXaxis()->GetBinCenter(etaBin);
    for (Int_t phiBin = 1; phiBin <= centralDist->GetNbinsX(); phiBin++) {
      dNdeta->Fill(eta,cent,centralDist->GetBinContent(etaBin,phiBin));
    }
  }
  for (Int_t etaBin = 1; etaBin <= forwardDist->GetNbinsX(); etaBin++) {
    Double_t eta = forwardDist->GetXaxis()->GetBinCenter(etaBin);
    for (Int_t phiBin = 1; phiBin <= forwardDist->GetNbinsX(); phiBin++) {
      dNdeta->Fill(eta,cent,forwardDist->GetBinContent(etaBin,phiBin));
    }
  }

  Double_t zvertex = fUtil.GetZ();

  if (fSettings.makeFakeHoles) fUtil.MakeFakeHoles(*forwardDist);

  static_cast<TH1D*>(fEventList->FindObject("Centrality"))->Fill(cent);
  static_cast<TH1D*>(fEventList->FindObject("Vertex"))->Fill(zvertex);
  AliForwardGenericFramework calculator = AliForwardGenericFramework();
  //AliForwardQCumulantRun2 qc_calculator = AliForwardQCumulantRun2();
  
  calculator.fSettings = fSettings;

  //if (!fSettings.stdQC){
  if (fSettings.a5){
    calculator.CumulantsAccumulate(*refDist, fOutputList, cent, zvertex,"forward",true,false);

    TH2D refDist_tmp = TH2D("r","",400,-1.5,1.5,400,0,2*TMath::Pi());
    refDist_tmp.SetDirectory(0);
    refDist     = &refDist_tmp;

    refDist     ->SetDirectory(0);
    fUtil.FillDataCentral(refDist);
    calculator.CumulantsAccumulate(*refDist, fOutputList, cent, zvertex,"central",true,false);
  }
  else{
    if (fSettings.ref_mode & fSettings.kFMDref) calculator.CumulantsAccumulate(*refDist, fOutputList, cent, zvertex,"forward",true,false);
    else calculator.CumulantsAccumulate(*refDist, fOutputList, cent, zvertex,"central",true,false);
  }
  calculator.CumulantsAccumulate(*forwardDist, fOutputList, cent, zvertex,"forward",false,true);
  // }
  // else {
  //   qc_calculator.CumulantsAccumulate(*centralDist, fOutputList, cent, zvertex,"central");
  //   qc_calculator.CumulantsAccumulate(*forwardDist, fOutputList, cent, zvertex,"forward");
  // }


  // if (!fSettings.stdQC){
  //   Int_t ptnmax =  (fSettings.doPt ? 9 : 0);

  //   TH1F pthist = TH1F("pthist", "", ptnmax+1, fSettings.minpt, fSettings.maxpt);

  //   if (fSettings.doPt){
  //     for (Int_t ptn = 0; ptn <=ptnmax; ptn ++ ){
        
  //       fUtil.fSettings.minpt = pthist.GetXaxis()->GetBinLowEdge(ptn+1);
  //       fUtil.fSettings.maxpt = pthist.GetXaxis()->GetBinUpEdge(ptn+1);

  //       centralDist->Reset();

  //       // Fill centralDist
  //       fUtil.FillDataCentral(centralDist);

  //       UInt_t randomInt = fRandom.Integer(fSettings.fnoSamples);

  //       calculator.CumulantsAccumulate(*centralDist, fOutputList, cent, zvertex,"central",false,true);  
  //       calculator.saveEvent(fOutputList, cent, zvertex,  randomInt, ptn);    
  //       calculator.fpvector->Reset();
  //     }
  //   }
  //   else{
      calculator.CumulantsAccumulate(*centralDist, fOutputList, cent, zvertex,"central",false,true);  
      UInt_t randomInt = fRandom.Integer(fSettings.fnoSamples);
      calculator.saveEvent(fOutputList, cent, zvertex,  randomInt, 0);   
  //   }
  // }
  // else{
  //   UInt_t randomInt = fRandom.Integer(fSettings.fnoSamples);
  //   qc_calculator.saveEvent(fOutputList, cent, zvertex,  randomInt);//, 0);  
  // }

  // centralDist->Reset();
  // forwardDist->Reset();
  // refDist->Reset();
  // calculator.reset();
  //qc_calculator.reset();
  PostData(1, fOutputList);

  return;
}


//_____________________________________________________________________
void AliForwardFlowRun2Task::Terminate(Option_t */*option*/)
{
  std::cout << "Terminating" << '\n';
  return;
}


//_______________________________________________________________
