#include<iostream>
#include<cmath>
#include<fstream>
#include<sstream>
#include"solarflux.h"
#include"crosssections.h"
#include"detectors.h"
#include"general_adiabatic.h"
#include"constant_matter_evolution.h"
#include"statistics.h"

using namespace std;

double Xsection(double Enu, double T, void *p){
	zprime_param zp = *(zprime_param *)p;
	zp.Enu = Enu;
	zp.T = T;
	return(nueScatteringJUNO(zp));
}

double survivalProb_param(double Enu, void *p){
	osc_param osc_p = *(osc_param *)p;
	double Rfrac = 0.05;

	//double Pee = survivalProb_3genSun_Ad_freeparam(Enu, Rfrac, osc_p);//*sun_earth_prob_2layers(Rfrac, Enu, osc_p);
	//double Pee = survivalProb_3genSun_Ad_freeparam(Enu, Rfrac, osc_p);
	double Pee = sun_earth_prob_5layers(Rfrac, Enu, osc_p);

	if(osc_p.prob_ee)
		return(Pee);
	return(1 - Pee);
}

double Flux(double Enu){
	return(solarFlux(3, Enu)/*8B*/+solarFlux(2, Enu)/*hep*/);
}

double Ntau_func(double E){
	double Nelectrons1 = 7.9*3.38*pow(10,32);
	double Nelectrons2 = 12.2*3.38*pow(10,32);
	double Nelectrons3 = 16.2*3.38*pow(10,32);
	double year = 3.154*pow(10,7);
	year *= 10;
	double Ntau1 = Nelectrons1*year;
	double Ntau2 = Nelectrons2*year;
	double Ntau3 = Nelectrons3*year;
	double Ntau;
	if(E <= 3)
		Ntau = Ntau1*0.51;
	else if(E <= 5)
		Ntau = Ntau2*0.41;//Bi-Tl correlation cut
	else
		Ntau = Ntau3*0.52;
	return(Ntau);
}

void calcula_eventos_Bins_duplos(double *N, signal_param sig, detector_param detec){
	osc_param op = *(osc_param *)sig.oscillation_param;
	double binsize = 2.0/op.n_exp_bins;
	//op.cosz = binsize/2 - 1;
	op.cosz = -1;
	double N_1d[210];
	for(int i = 0; i < op.n_exp_bins; i++){
		op.i = i;
		sig.oscillation_param = (void *)(&op);
		calcula_eventos_Bins(N_1d, sig, detec);
		for(int j = 0; j < detec.numberOfBins; j++){
			//N[i*detec.numberOfBins + j] = op.exposure[i]*N_1d[j];
			N[i*detec.numberOfBins + j] = (1.0/op.n_exp_bins)*N_1d[j];
		}
		op.cosz += binsize;
	}
}

int main(){
	/***********************/	
	/*OUTPUT FILE DIRECTORY*/
	/***********************/
	string outdir = "results/";

	ofstream saida;
	string file_name = "junoevents_daynight.txt";
	saida.open(outdir + file_name);

	int n_E_Bins = 140;
	int n_cosz_Bins = 100;
	double binMin = 2;
	double binMax = 16;
	double binsize = (binMax - binMin)/n_E_Bins;
	double Eres = 3;

	double *T = new double [n_E_Bins*n_cosz_Bins];
	double *N_obs = new double [n_E_Bins*n_cosz_Bins];
	double *N_BG = new double [n_E_Bins*n_cosz_Bins];
	double *N_BG_small = new double [n_E_Bins];
	double *exposure = new double [n_cosz_Bins];

	zprime_param zp;
	zp.antinu = false;
	zp.gX = zp.a = zp.e = zp.mZprime = zp.QlL = zp.QlR = zp.QnuL = 0;

	string exposure_file = "JUNO_exposure_100bins.txt";
	seta_exposure(exposure, n_cosz_Bins, exposure_file);
	seta_dados_Ne();

	osc_param osc_p;
	osc_p.seta_osc_param(false);
	osc_p.n_exp_bins = n_cosz_Bins;
	osc_p.exposure = exposure;

	osc_p.theta12 = asin(sqrt(0.307));
	osc_p.Dmq21 = 4.8e-5;
	osc_p.constroi_PMNS();
	
	signal_param sig;
	sig.flux = &Flux;
	sig.max_flux_E = 18.79; /*maximum solar flux energy*/
	sig.xsection = &Xsection;
	sig.xsection_param = (void *)(&zp);
	sig.oscillation_prob = &survivalProb_param;
	sig.oscillation_param = (void *)(&osc_p);

	detector_param detec;
	detec.numberOfBins = n_E_Bins;
	detec.Ebinmin = binMin;
	detec.Ebinmax = binMax;
	detec.binsize = binsize;
	detec.Eres = Eres;
	detec.Ntau = &Ntau_func;

	calcula_eventos_Bins_duplos(T, sig, detec);
	cout << "Calculei o primeiro set!" << endl;

	string BG_file = "JUNO_BG_total.txt";
	seta_BG(N_BG_small, n_E_Bins, BG_file);
	for(int i = 0; i < n_E_Bins; i++){
		for(int j = 0; j < n_cosz_Bins; j++){
			N_BG[i + j*n_E_Bins] = (1.0/n_cosz_Bins)*N_BG_small[i];
		}
	}

	//osc_p.theta12 = asin(sqrt(0.107));
	//osc_p.Dmq21 = 4.8e-5;
	osc_p.seta_osc_param(false);
	osc_p.constroi_PMNS();
	sig.oscillation_param = (void *)(&osc_p);
	
	calcula_eventos_Bins_duplos(N_obs, sig, detec);
	cout << "Calculei o segundo set!" << endl;
		
	for(int i = 0; i < n_E_Bins*n_cosz_Bins; i++)
		N_obs[i] += N_BG[i];

	stat_params p;
	p.nBins = n_E_Bins*n_cosz_Bins;
	p.T = T;
	p.N_obs = N_obs;
	p.BG = N_BG;

	p.sd = 0.05;
	p.sb = 0.15;

	chisquare_twopulls_JUNO(p, true);

	double E = binMin, eventos;

	for(int i = 0; i < n_E_Bins; i++){
		eventos = 0;
		for(int j = 0; j < n_cosz_Bins; j++){
			eventos += N_obs[i + j*n_E_Bins];
			//saida << i << " " << j << " " << T[i + j*n_E_Bins] << endl;
		}
		saida << E << " " << eventos << endl;
		E += binsize;
		saida << E << " " << eventos << endl;
	}

	delete [] T;
	delete [] N_obs;
	delete [] N_BG;
	delete [] N_BG_small;
	delete [] exposure;

	saida.close();
	return(0);
}