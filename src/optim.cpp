/*==============================================================================

                                 r  i  t  a

            An environment for Modelling and Numerical Simulation

  ==============================================================================

    Copyright (C) 2021 Rachid Touzani

    This file is part of rita.

    rita is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    rita is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

  ==============================================================================

                        Implementation of class 'optim'

  ==============================================================================*/


#include "optim.h"
#include "cmd.h"
#include "data.h"

namespace RITA {

optim::optim(rita*      r,
             cmd*       command,
             configure* config)
      : _rita(r), _configure(config), _cmd(command)
{
   _verb = _rita->_verb;
   log = true;
   nb_lec = nb_gec = nb_eqc = 0;
   G_ok = H_ok = solved = lp = false;
   penal = 1./OFELI_TOLERANCE;
}


optim::~optim()
{
}


int optim::run()
{
   _rita->_analysis_type = OPTIMIZATION;
   size = 1;
   int nb_le=0, nb_ge=0, nb_eq=0;
   int count_fct=0, count_obj=0, count_grad=0, count_hess=0, count_init=0, count_lp=0;
   int count_lec=0, count_gec=0, count_eqc=0, count_field=0, penal_ok=0, nb=0, ind=0, key=0;
   double in=0., x=0.;
   vector<string> grad, hess, var, le_cons, eq_cons;
   OFELI::Vect<double> *A_le, *A_ge, *A_eq;
   string str, J, name="", var_name, method="gradient", constr="";
   data *theData = _rita->_data;
   init.clear();
   grad.clear();
   hess.clear();
   map<int,double> llb, uub;
   const vector<string> kw = {"help","?","set","size","func$tion","obj$ective","lp","grad$ient","hess$ian",
                              "low$-bound","up$-bound","ge$-constraint","le$-constraint","eq$-constraint",
                              "penal$ty","var$iable","field","init$ial","algo$rithm","summary","clear",
                              "end","<","quit","exit"};
   _cmd->set(kw);
   int nb_args = _cmd->getNbArgs();
   for (int k=0; k<nb_args; ++k) {

      int n = _cmd->getArgs(nb);
      switch (n) {

         case 3:
            size = _cmd->int_token(0);
            break;

         case 4:
            name = _cmd->string_token(0);
            count_fct++;
            break;

         case 5:
            J = _cmd->string_token(0);
            count_obj++;
            break;

         case 6:
            lp = true;
            break;

         case 7:
            for (int i=0; i<nb; ++i) {
               grad.push_back(_cmd->string_token(i));
               count_grad++;
            }
            break;

         case 8:
            for (int i=0; i<nb; ++i) {
               hess.push_back(_cmd->string_token(i));
               count_hess++;
            }
            break;

         case 9:
            llb[_cmd->int_token(0)] = _cmd->double_token(1);
            break;

         case 10:
            uub[_cmd->int_token(0)] = _cmd->double_token(1);
            break;

         case 15:
         case 16:
            var_name = _cmd->string_token(0);
            count_field++;
            break;

         case 17:
            for (int i=0; i<nb; ++i) {
               in = _cmd->double_token(i);
               init.push_back(in);
               count_init++;
            }
            break;

         case 18:
            method = _cmd->string_token(0);
            break;

         default:
            cout << "Error: Unknown argument: " << _cmd->Arg() << endl;
            *_ofl << "In rita>optimization>: Unknown argument: " << _cmd->Arg() << endl;
	    return 1;
      }
   }

   if (nb_args>0) {
      if (size<=0) {
         cout << "Error: Illegal size value." << endl;
         *_ofl << "In rita>optimization>: Illegal size value." << endl;
         return 1;
      }
      if (!count_obj && !count_fct) {
         cout << "Error: Missing objective function." << endl;
         *_ofl << "In rita>optimization>: Missing objective function." << endl;
         return 1;
      }
      if (count_fct && count_field) {
         cout << "Error: Function already defined in data module." << endl;
         *_ofl << "In rita>optimization>: Function already defined in data module." << endl;
         return 1;
      }
      if (count_fct && count_obj) {
         cout << "Error: Function already defined." << endl;
         *_ofl << "In rita>optimization>: Function already defined." << endl;
         return 1;
      }
      if (count_fct>1 || count_obj>1) {
         cout << "Error: Too many objective functions defined." << endl;
         *_ofl << "In rita>optimization>: Too many objective functions defined." << endl;
         return 1;
      }
      if (count_obj && !count_field) {
         cout << "Error: Missing a variable name." << endl;
         *_ofl << "In rita>optimization>: Missing a variable name." << endl;
         return 1;
      }
      if (count_init>size) {
         cout << "Error: Illegal number of initial guesses given." << endl;
         *_ofl << "In rita>optimization>: Too many initial guesses given." << endl;
         return 1;
      }
      *_ofh << "optimization";
      if (count_fct>0) {
         ind = theData->checkFct(name);
         if (ind==-1) {
            cout << "Error: Non defined function " << name << endl;
            *_ofl << "In rita>optimization>: Non defined function " << name << endl;
            return 1;
         }
         J_Fct = theData->theFct[ind];
         if (J_Fct->set(name,theData->theFct[ind]->expr,theData->theFct[ind]->var,1)) {
            cout << "Error in function evaluation: " << J_Fct->getErrorMessage() << endl;
            *_ofl << "In rita>optimization>: Error in function evaluation: "
                  << J_Fct->getErrorMessage() << endl;
            return 1;
         }
         *_ofh << " function=" << name;
      }
      else {
         ifield = theData->addField(var_name,GIVEN_SIZE,size);
         if (size==1)
            var.push_back(var_name);
         else {
            for (int i=0; i<size; ++i)
               var.push_back(var_name+to_string(i+1));
         }
         *_ofh << " var=" << var_name;
         theData->addFunction("",J,var);
         theData->FieldType[ifield] = OPT;
         J_Fct = theData->theFct[theData->getNbFcts()-1];
         if (count_grad) {
            if (count_grad!=size) {
               cout << "Error: Illegal number of gradient components given." << endl;
               *_ofl << "In rita>optimization>: Illegal number of gradient components given." << endl;
               return 1;
            }
            G_ok = true;
            for (int i=0; i<size; ++i) {
               igrad = theData->addFunction("",grad[i],var);
               G_Fct.push_back(theData->theFct[theData->getNbFcts()-1]);
            }
            *_ofh << " gradient=" << grad[0];
            for (int i=1; i<size-1; ++i)
               *_ofh << grad[i] << ",";
            *_ofh << grad[size-1];
         }
         if (count_hess) {
            if (count_hess!=size*size) {
               cout << "Error: Illegal number of hessian components given." << endl;
               *_ofl << "In rita>optimization>: Illegal number of hessian components given." << endl;
               return 1;
            }
            H_ok = true;
            for (int i=0; i<size; ++i) {
               for (int j=0; j<size; ++j) {
                  ihess = theData->addFunction("",hess[size*i+j],var);
                  H_Fct.push_back(theData->theFct[theData->getNbFcts()]);
	       }
	    }
            *_ofh << " hessian=" << hess[0];
            for (int i=1; i<size*size-1; ++i)
               *_ofh << hess[i] << ",";
            *_ofh << hess[size*size-1];
         }
         for (int i=0; i<size; ++i) {
            lb.push_back(-std::numeric_limits<real_t>::max());
            ub.push_back( std::numeric_limits<real_t>::max());
         }
         for (const auto& v: llb) {
            lb[v.first-1] = v.second;
            *_ofh << " low-bound=" << v.first << "," << v.second;
         }
         for (const auto& v: uub) {
            ub[v.first-1] = v.second;
            *_ofh << " up-bound=" << v.first << "," << v.second;
         }
         Alg = Nopt[method];
         *_ofh << " method=" << method;
         for (int i=count_init; i<size; ++i)
            init.push_back(0.);
         *_ofh << " init=" << init[0];
         for (int i=0; i<size; ++i)
            *_ofh << "," << init[i+1];
         for (int i=0; i<size; ++i)
            theData->u[ifield][i] = init[i];
         *_ofh << endl;
      }
      log = 0;
   }

   else {
      while (1) {
         nb = _cmd->readline("rita>optimization> ");
         if (nb<0)
            continue;
         switch (key=_cmd->getKW(kw)) {

            case 0:
            case 1:
               _cmd->setNbArg(0);
               cout << "\nAvailable Commands:\n";
               cout << "size:          Size of optimization problem (Number of optimization variables)\n";
               cout << "function:      Give function (already defined) as objective (cost) function\n";
               cout << "objective:     Give objective (cost) function expression\n";
               cout << "lp:            Define linear programming objective (cost)\n";
               cout << "gradient:      Define gradient of objective function\n";
               cout << "hessian:       Define hessian of objective function\n";
               cout << "low-bound:     Define a lower bound for a given variable as constraint\n";
               cout << "up-bound:      Define an upper bound for a given variable as constraint\n";
               cout << "ge-constraint: Define a (>=) inequality constraint (for Linear Programming problems only)\n";
               cout << "le-constraint: Define a (<=) inequality constraint\n";
               cout << "eq-constraint: Define an equality constraint\n";
               cout << "penalty:       Penalty parameter (small) to enforce constraints\n";
               cout << "variable:      Variable (or Field) name as unknown of the optimization problem\n";
               cout << "init:          Initial guess for iterations\n";
               cout << "algorithm:     Set optimization algorithm\n";
               cout << "summary:       Summary of optimization problem attributes\n";
               cout << "clear:         Clear optimization problem settings\n";
               cout << "end or <:      go back to higher level" << endl;
               break;

            case 2:
               _rita->setConfigure();
               break;

            case 3:
               if (size<=0) {
                  cout << "Illegal size value." << endl;
                  *_ofl << "In rita>optimization>size>: Illegal size value." << endl;
                  _ret = 1;
	          break;
               }
               if (_cmd->get(size)) {
                  _ret = 1;
                  break;
               }
               _rita->_ret = 0;
               break;

            case 4:
               if (lp) {
                  cout << "Error: This argument is not compatible with linear programming." << endl;
                  *_ofl << "In rita>optimization>function>: Argument not compatible with linear programming." << endl;
                  _ret = 1;
                  break;
               }
               if (_cmd->setNbArg(1,"Function name to be given.")) {
                  *_ofl << "In rita>optimization>function>: Missing function name." << endl;
                  _ret = 1;
                  break;
               }
               if (_cmd->get(name)) {
                  _ret = 1;
                  break;
	       }
               count_fct++;
               _ret = 0;
               break;

            case 5:
              if (lp) {
                 if (count_lp==size+1) {
                    cout << "Error: Too many objective function data." << endl;
                    *_ofl << "In rita>optimization>objective>: Too many objective function data." << endl;
                    break;
                  }
                  while (!_cmd->get(x)) {
                     if (count_lp==size)
                        b = x;
                     else
                        a.push_back(x);
                     count_lp++;
		  }
              }
              else {
                 if (_cmd->setNbArg(1,"Objective function to be given.",1)) {
                    *_ofl << "In rita>optimization>objective>: Missing objective function expression." << endl;
                    _ret = 1;
                    break;
                  }
                  if (_cmd->get(J)) {
                     _ret = 1;
                     break;
                  }
                  count_obj++;
               }
               _ret = 0;
               break;

            case 6:
               if (count_fct || count_grad || count_hess || penal_ok) {
                  cout << "Error: Linear programming option incompatible other arguments." << endl;
                  *_ofl << "In rita>optimization>lp>: Linear programming option incompatible with other arguments." << endl;
               }
               lp = true;
               count_lec = count_gec = count_eqc = 0;
               break;

            case 7:
               if (lp) {
                  cout << "Error: This argument is not compatible with linear programming." << endl;
                  *_ofl << "In rita>optimization>gradient>: Argument not compatible with linear programming." << endl;
                  _ret = 1;
                  break;
               }
               if (count_grad==size) {
                  cout << "Error: Too many gradient definitions." << endl;
                  *_ofl << "In rita>optimization>gradient>: Too many gradient definitions." << endl;
               }
               if (_cmd->setNbArg(size,"Partial derivative of objective function to be given.")) {
                  *_ofl << "In rita>optimization>gradient>: Missing partial derivatives of objective function." << endl;
                  _ret = 1;
                  break;
               }
               _ret = _cmd->get(str);
               grad.push_back(str);
               count_grad++;
               _ret = 0;
               break;

            case 8:
               if (lp) {
                  cout << "Error: This argument is not compatible with linear programming." << endl;
                  *_ofl << "In rita>optimization>hessian>: Argument not compatible with linear programming." << endl;
                  _ret = 1;
                  break;
               }
               if (count_hess==size*size) {
                  cout << "Error: Too many hessian definitions." << endl;
                  *_ofl << "In rita>optimization>hessian>: Too many functions defining hessian." << endl;
                  _ret = 1;
                  break;
               }
               if (_cmd->setNbArg(size,"Partial derivatives of gradient to be given.")) {
                  *_ofl << "In rita>optimization>hessian>: Missing partial derivatives of gradient." << endl;
                  _ret = 1;
                  break;
               }
               _ret = _cmd->get(str);
               hess.push_back(str);
               count_hess++;
               _ret = 0;
               break;

            case 9:
               if (lp) {
                  cout << "Error: This argument is not compatible with linear programming." << endl;
                  *_ofl << "In rita>optimization>low-bound>: Argument not compatible with linear programming." << endl;
                  _ret = 1;
                  break;
               }
               if (_cmd->setNbArg(2,"Lower value to enforce to be given.")) {
                  *_ofl << "In rita>optimization>low-bound>: Lower value to enforce to be given." << endl;
                  break;
               }
               _ret = _cmd->get(ind);
               _ret = _cmd->get(x);
               llb[ind] = x;
               break;

            case 10:
               if (lp) {
                  cout << "Error: This argument is not compatible with linear programming." << endl;
                  *_ofl << "In rita>optimization>up-bound>: Argument not compatible with linear programming." << endl;
                  _ret = 1;
                  break;
               }
               if (_cmd->setNbArg(2,"Upper value to enforce to be given.")) {
                  *_ofl << "In rita>optimization>up-bound>: Upper value to enforce to be given." << endl;
                  break;
               }
               _ret = _cmd->get(ind);
               _ret = _cmd->get(x);
               uub[ind] = x;
               break;

            case 11:
               if (!lp) {
                  cout << "Error: This argument is valid for linear programming problems only." << endl;
                  *_ofl << "In rita>optimization>ge-constraint>: This argument is valid for linear programming problems only." << endl;
                  _ret = 1;
                  break;
               }
               while (!_cmd->get(x)) {
                  if (nb_ge==size) {
                     a_ge.push_back(A_ge);
                     b_ge.push_back(x);
                     nb_ge = 0;
                  }
                  else {
                     if (nb_ge==0)
                        A_ge = new OFELI::Vect<double>(size);
                     (*A_ge)[nb_ge++] = x;
                  }
                  count_gec++;
               }
               break;

            case 12:
               if (lp) {
                  while (!_cmd->get(x)) {
                     if (nb_le==size) {
                        a_le.push_back(A_le);
                        b_le.push_back(x);
                        nb_le = 0;
                     }
                     else {
                        if (nb_le==0)
                           A_le = new OFELI::Vect<double>(size);
                        (*A_le)[nb_le++] = x;
                     }
                     count_lec++;
                  }
                  break;
               }
               _ret = _cmd->get(str);
               le_cons.push_back(str);
               count_lec++;
               break;

            case 13:
               if (lp) {
                  while (!_cmd->get(x)) {
                     if (nb_eq==size) {
                        a_eq.push_back(A_eq);
                        b_eq.push_back(x);
                        nb_eq = 0;
                     }
                     else {
                        if (nb_eq==0)
                           A_eq = new OFELI::Vect<double>(size);
                        (*A_eq)[nb_eq++] = x;
                     }
                     count_eqc++;
                  }
                  break;
               }
               _ret = _cmd->get(str);
               eq_cons.push_back(str);
               count_eqc++;
               break;

            case 14:
               if (lp) {
                  cout << "Error: This argument is not compatible with linear programming." << endl;
                  *_ofl << "In rita>optimization>penal>: Argument not compatible with linear programming." << endl;
                  _ret = 1;
                  break;
               }
               _ret = _cmd->get(penal);
               penal_ok++;
               break;
 
            case 15:
            case 16:
               if (_cmd->setNbArg(1,"Give name of associated field.")) {
                  *_ofl << "In rita>optimization>variable>: Missing name of associated field." << endl;
                  break;
               }
               _cmd->get(str);
               var_name = str;
               count_field++;
               break;

            case 17:
               if (lp) {
                  cout << "Error: This argument is not necessary for linear programming." << endl;
                  *_ofl << "In rita>optimization>initial>: Argument not necessary for linear programming." << endl;
                  _ret = 1;
                  break;
               }
               if (count_init==size) {
                  cout << "Error: Too many initial solution definitions." << endl;
                  *_ofl << "In rita>optimization>initial>: Too many initial solutions definitions." << endl;
	          break;
               }
               if (_cmd->setNbArg(size,1,"Initial guess to be given.",1)) {
                  *_ofl << "In rita>optimization>initial>: Missing initial guess." << endl;
                  break;
               }
               while (!_cmd->get(x))
                  init.push_back(x), count_init++;
               break;

            case 18:
               if (_log) {
                  cout << "Please define algebraic equation first." << endl;
                  *_ofl << "In rita>optimization>algorithm>: equation must be set first." << endl;
                  break;
               }
               if (_cmd->setNbArg(1,"Optimization algorithm to be supplied.",1)) {
                  *_ofl << "In rita>optimization>algorithm>: Missing optimization algorithm." << endl;
                  break;
               }
               _ret = _cmd->get(_alg);
               if (Nopt.find(_alg) == Nopt.end() ) {
                  cout << "Error: Unknown or nonimplemented optimization algorithm: " << _alg << endl;
                  *_ofl << "In rita>optimization>algorithm>: Unknown or nonimplemented optimization algorithm: " << _alg << endl;
               }
               _rita->_ret = 0;
               break;

            case 19:
               cout << "Summary of optimization problem attributes:\n";
               _rita->_ret = 0;
               break;

            case 20:
               size = 0;
               G_ok = H_ok = 0;
               *_ofh << "  clear" << endl;
               cout << "Optimization problem cleared." << endl;
               break;

            case 21:
            case 22:
               _cmd->setNbArg(0);
               if (lp) {
                  if (!count_lp && lp) {
                     cout << "Error: Missing objective function." << endl;
                     *_ofl << "In rita>optimization>end>: Missing objective function." << endl;
                     return 1;
                     break;
                  }
                  if (!count_field) {
                     cout << "Error: Missing a variable name." << endl;
                     *_ofl << "In rita>optimization>end>: Missing a variable name." << endl;
                     break;
                  }
                  *_ofh << "optimization\n  size " << size << endl << "  lp" << endl;
                  ifield = theData->addField(var_name,GIVEN_SIZE,size);
                  theData->FieldType[ifield] = OPT;
                  if (size==1)
                     var.push_back(var_name);
                  else {
                     for (int i=0; i<size; ++i)
                        var.push_back(var_name+to_string(i+1));
                  }
                  *_ofh << "  variable " << var_name << endl;
                  *_ofh << "  objective ";
                  for (int i=0; i<size; ++i)
                     *_ofh << a[i] << " ";
                  *_ofh << b << endl;
                  nb_lec = a_le.size(), nb_gec = a_ge.size(), nb_eqc = a_eq.size();
                  for (int i=0; i<nb_eqc; ++i) {
                     *_ofh << "  eq-constraint  ";
                     for (int j=0; j<size; ++j)
                        *_ofh << (*a_eq[i])[j] << " ";
                     *_ofh << b_eq[i] << endl;
                  }
                  for (int i=0; i<nb_lec; ++i) {
                     *_ofh << "  le-constraint  ";
                     for (int j=0; j<size; ++j)
                        *_ofh << (*a_le[i])[j] << " ";
                     *_ofh << b_le[i] << endl;
                  }
                  for (int i=0; i<nb_gec; ++i) {
                     *_ofh << "  ge-constraint  ";
                     for (int j=0; j<size; ++j)
                        *_ofh << (*a_ge[i])[j] << " ";
                     *_ofh << b_ge[i] << endl;
                  }
               }
	       else {
                  if (!count_obj && !count_fct) {
                     cout << "Error: Missing objective function." << endl;
                     *_ofl << "In rita>optimization>end>: Missing objective function." << endl;
                     break;
                  }
                  if (count_fct && count_field) {
                     cout << "Error: Function already defined in data module." << endl;
                     *_ofl << "In rita>optimization>end>: Function already defined in data module." << endl;
                     break;
                  }
                  if (count_fct && count_obj) {
                     cout << "Error: Function already defined." << endl;
                     *_ofl << "In rita>optimization>end>: Function already defined." << endl;
                     return 1;
                  }
                  if (count_fct>1 || count_obj>1) {
                     cout << "Error: Too many objective functions defined." << endl;
                     *_ofl << "In rita>optimization>end>: Too many objective functions defined." << endl;
                     break;
                  }
                  if (count_obj && !count_field) {
                     cout << "Error: Missing a variable name." << endl;
                     *_ofl << "In rita>optimization>end>: Missing a variable name." << endl;
                     break;
                  }
                  *_ofh << "optimization\n  size " << size << endl;
                  if (count_fct>0) {
                     ind = theData->checkFct(name);
                     if (ind==-1) {
                        cout << "Error: Non defined function " << name << endl;
                        *_ofl << "In rita>optimization>end>: Non defined function " << name << endl;
                        _ret = 1;
                        break;
                     }
                     J_Fct = theData->theFct[ind];
                     if (J_Fct->set(name,theData->theFct[ind]->expr,theData->theFct[ind]->var,1)) {
                        cout << "Error in function evaluation: " << J_Fct->getErrorMessage() << endl;
                        *_ofl << "In rita>optimization>end>: Error in function evaluation: "
                              << J_Fct->getErrorMessage() << endl;
                        _ret = 1;
                        break;
                     }
                     *_ofh << "  function " << name << endl;
                  }
                  else {
                     ifield = theData->addField(var_name,GIVEN_SIZE,size);
                     if (size==1)
                        var.push_back(var_name);
                     else {
                        for (int i=0; i<size; ++i)
                           var.push_back(var_name+to_string(i+1));
                     }
                     *_ofh << "  variable " << var_name << endl;
                     *_ofh << "  objective " << J << endl;
                     theData->addFunction("",J,var);
                     theData->FieldType[ifield] = OPT;
                     J_Fct = theData->theFct[theData->getNbFcts()-1];
                  }
                  if (count_grad) {
                     if (count_grad!=size) {
                        cout << "Error: Illegal number of gradient components given." << endl;
                        *_ofl << "In rita>optimization>: Illegal number of gradient components given." << endl;
                        break;
                     }
                     G_ok = true;
                     for (int i=0; i<size; ++i) {
                        igrad = theData->addFunction("",grad[i],var);
                        G_Fct.push_back(theData->theFct[theData->getNbFcts()-1]);
                     }
                     *_ofh << "  gradient  ";
                     for (int i=0; i<size; ++i)
                        *_ofh << grad[i] << " ";
                     *_ofh << endl;
                  }
                  nb_lec = count_lec, nb_eqc = count_eqc;
                  for (int i=0; i<nb_lec; ++i) {
                     iincons = theData->addFunction("",le_cons[i],var);
                     inC_Fct.push_back(theData->theFct[theData->getNbFcts()-1]);
                     *_ofh << "  le-constraint  " << le_cons[i] << endl;
                  }
                  for (int i=0; i<nb_eqc; ++i) {
                     ieqcons = theData->addFunction("",eq_cons[i],var);
                     eqC_Fct.push_back(theData->theFct[theData->getNbFcts()-1]);
                     *_ofh << "  eq-constraint  " << eq_cons[i] << endl;
                  }
                  if (penal_ok)
                     *_ofh << "  penalty " << penal << endl;
                  if (count_hess) {
                     if (count_hess!=size*size) {
                        cout << "Error: Illegal number of hessian components given." << endl;
                        *_ofl << "In rita>optimization>end>: Illegal number of hessian components given." << endl;
                        return 1;
                     }
                     H_ok = true;
                     *_ofh << "  hessian  ";
                     for (int i=0; i<size; ++i) {
                        for (int j=0; j<size; ++j) {
                           ihess = theData->addFunction("",hess[size*i+j],var);
                           H_Fct.push_back(theData->theFct[theData->getNbFcts()]);
                           *_ofh << hess[size*i+j] << " ";
                        }
	             }
                     *_ofh << endl;
                  }
                  for (int i=0; i<size; ++i) {
                     lb.push_back(-std::numeric_limits<real_t>::max());
                     ub.push_back( std::numeric_limits<real_t>::max());
                  }
                  if (llb.size()) {
                     for (const auto& v: llb) {
                        lb[v.first-1] = v.second;
                        *_ofh << "  low-bound  " << v.first << " " << v.second;
                     }
	          }
                  if (uub.size()) {
                     for (const auto& v: uub) {
                        ub[v.first-1] = v.second;
                        *_ofh << "  up-bound  " << v.first << " " << v.second;
                     }
	          }
                  Alg = Nopt[method];
                  *_ofh << "  algorithm " << method << endl;
                  for (int i=count_init; i<size; ++i)
                     init.push_back(0.);
                  *_ofh << "  init  ";
                  for (int i=0; i<size; ++i) {
                     (*theData->u[ifield])[i] = init[i];
                     *_ofh << init[i] << "  ";
                  }
	       }
               *_ofh << "\n  end" << endl;
               log = 0;
               _rita->_ret = 0;
               return 0;

            case 23:
            case 24:
               _rita->_ret = 100;
               return _ret;

            case -2:
            case -3:
            case -4:
               break;

            default:
               cout << "Unknown Command: " << _cmd->token() << endl;
               cout << "Available commands: size, objective, lp, gradient, hessian, low-bound, up-bound" << endl;
	       cout << "                    ineq-constraint, eq-constraint, penalty, variable, init, algorithm" << endl;
	       cout << "                    summary, clear, end, <" << endl;
               cout << "Global commands:    help, ?, set, quit, exit" << endl;
               *_ofl << "In rita>optimization>: Unknown Command " << _cmd->token() << endl;
               break;
         }
      }
   }
   _rita->_ret = 0;
   return 0;
}

} /* namespace RITA */
