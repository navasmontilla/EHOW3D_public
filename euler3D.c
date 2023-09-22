/*

Authors:
 - Adrián Navas Montilla
 - Isabel Echeverribar

Copyright (C) 2018-2019 The authors.  

License type: Creative Commons Attribution-NonCommercial-NoDerivs 3.0 Spain (CC BY-NC-ND 3.0 ES https://creativecommons.org/licenses/by-nc-nd/3.0/es/deed.en) under the following terms: 

- Attribution — You must give appropriate credit and provide a link to the license.
- NonCommercial — You may not use the material for commercial purposes.
- NoDerivatives — If you remix, transform, or build upon the material, you may not distribute the modified material unless explicit permission of the authors is provided. 

Disclaimer: This software is distributed for research and/or academic purposes, WITHOUT ANY WARRANTY. In no event shall the authors be liable for any claim, damages or other liability, arising from, out of or in connection with the software or the use or other dealings in this software.

*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <omp.h>

//Screen messages
#define END "\033[1;32m  =)\033[0m "
#define WAR "\033[1;33m [!]\033[0m "
#define ERR "\033[1;31m [ERROR]\033[0m "
#define OK "\033[1;35m [OK]\033[0m "

//Physical constants
#define PI 3.141592653589793
#define _g_ 9.8
#define _gamma_ 1.4
#define _R_ 287.058
#define _p0_ 1.0E5

//Useful functions
#define TOL4 1.0E-4
#define TOL8 1.0E-8
#define TOL14 1.0E-14
#define TOL40 1.0E-40
#define MIN(x,y) (x < y ? x : y) 
#define MAX(x,y) (x > y ? x : y)
#define ABS(x) (x < 0 ? -x : x)

//reconstruction method 
#define TYPE_REC 1 //This is 0 for WENO, 1 for TENO and 2 for UWC
#define _CT_ 1.0e-6
#define epsilon  1.0E-6
#define epsilon2 1.0E-40
#define _Q_ 6.0

//Equations
#define LINEAR 0 //Linear scalar transport: f=a*u
#define BURGERS 0 //Burgers: f=u**2/2
#define LINEAR_TRANSPORT 0 //DO NOT USE THIS. Simulation of linear transport within the Euler solver. EULER must be = 1
#define EULER 1 //Euler equations
#define SW 0

//Source terms
#define ST 3// 0: Source OFF, 1: Source ON (augmented version, needs HLLS), 2: Source ON (perturbation version), 3: Source ON (perturbation version, total energy)

//Multicomponent and multiphase flow
#define MULTICOMPONENT 0 //Activates multicomponent Euler equations (two components with different gamma). 
#define MULTI_TYPE 2     //=1 for gamma formulation, =2 for 1/(gamma-1) formulation. ATENTION: Option =2 recommended (see R. Abgrall, S. Karni, Computations of Compressible Multifluids, JCP 169 (2001))

//Solvers
#define HLLE 1
#define HLLC 0
#define HLLS 0
#define ROE 0

//Debug code 
#define DEBUG_MESH 0 //0: no debug; 1: screen info; 

//Mesh stuff
#define ALLOW_SOLIDS 0 //0: no solid cells
#define _stol_ 2.0 //tolerance for the generation of ghost cell layers. 1.0: 1 layer, 2.0: 2 layers....

//Acceleration functions
#define NTHREADS 24

//Printing variables (vtk)
#define WRITE_VTK 1
#define print_RHO 0
#define print_VELOCITY 1
#define print_ENERGY 0
#define print_PRESSURE 0
#define print_OVERPRESSURE 1
#define print_SOLUTES 0
#define print_POTENTIALTEM 1

//Printing list of variables
#define WRITE_LIST 1
#define WRITE_TKE 0 //write file TKE evolution in time

//Reading data
#define READ_INITIAL 0

////////////////////////////////////////////////////
//////////////  S T R U C T U R E S  ///////////////
////////////////////////////////////////////////////


typedef struct t_node_ t_node;
typedef struct t_cell_ t_cell; 
typedef struct t_wall_ t_wall;
typedef struct t_mesh_ t_mesh;
typedef struct t_sim_ t_sim;
typedef struct t_solid_ t_solid;
typedef struct t_stl_ t_stl;
typedef struct t_triangle_ t_triangle;


struct t_node_{
	int id;
	double x,y,z; //coordinates of the node

};


struct t_cell_{
	int id;
	int l,m,n;//index in cartesian reference
		//double u,v,H,e_i,p; //primitive variables: density, x velocity, y velocity, specific enthalpy, specific internal energy, pressure
		//double rho,ru,rv,E,rp;//conserved variables: density, mass flow x, mass flow y, internal energy,  pasive scalar (rho*p);
		//double rho_aux,ru_aux,rv_aux,E_aux,rp_aux;//conserved variables: density, mass flow x, mass flow y, internal energy, pasive scalar (rho*p);
	double *U; //this is the array of vars.
			//When using Euler: rho, rhou, rhov, E, rhop
			//When using SW: h, hu, hv
	double *U_aux;
	double *Ue; //equilibrium state
	double *S;
      double *S_corr;
	double pres,prese,u_int; //pressure
	double dx,dy,dz;
	double xc,yc,zc;
	int n1,n2,n3,n4,n5,n6,n7,n8; //ID's of the nodes of the cell
	//        (4)------------(3)
	//         |		  |
	//         |		  |
	//        (1)------------(2)
	//
	int w1_id,w2_id,w3_id,w4_id,w5_id,w6_id; //ID's of the walls of the cell
	t_wall *w1, *w2, *w3, *w4, *w5, *w6;
	// 	      ------(3)-----
	//          |		 |
	//         (4)		(2)
	//          |		 |
	//          ------(1)-----

      int type; //0=solid, 1=nomal
      int ghost; //0=no, 1=yes
	double xim,yim,zim;
      int ni[8]; //id: for each ghost cell, neighbors of image points.
	double li[8],distabs;
      int distsolx,distsoly,distsolz; //distance to solid cells in cell units
      int solid_id,triangle_id;
      int out; //auxiliary variable for assing_cell_type() . It indicates that a cell is outside of a surface
      t_triangle *tri;
      int st_sizeX, st_sizeY, st_sizeZ;		//stencil size
	int stX[9], stY[9], stZ[9];		//id's of the cells in the X and Y stencil.
					//Initally set to 9 to avoid the use of malloc
					//But only filled up to st_size.
	
};


struct t_wall_{
	int id;
	int stencil; //this could be 1, 3, 5 or 7, depending on the method stencil  
		//double rhoL, ruL, rvL, EL, rpL, rhoR, ruR, rvR, ER, rpR;
		//extrapolated values at the left and right side of the wall, coming from (WENO) reconstruction

	double *UL, *UR; //array of extrapolated values at the left and right side of the wall, coming from (WENO) reconstruction
	double *fR_star,*fL_star;
      double *ULe, *URe; //array of extrapolated values at the left and right side of the wall, coming from (WENO) reconstruction, for the EQUILIBRIUM
	double pRe,pLe; //equilibrium pressures
	int cellR_id, cellL_id; //id of the right and left cell
	t_cell *cellR, *cellL; //pointers to the right and left cells of the wall
	double nx, ny, nz;
	double z; //height in z direction
      int wtype, boundId; //wtype: 1 for inner walls, 3 for transmissive boundary walls and 4 for solid walls
      double vel;
};



struct t_mesh_{
	int xcells, ycells, zcells; //values given by the user
    double dx, dy, dz;
      double Lx, Ly, Lz;
      double u_x, u_y, u_z;
	int ncells;
	int nwalls;
	int nnodes;
    int bc[6]; //boundary type
	int flux_bc_flag,cell_bc_flag; //cell_bc_flag is 1 if all the boundaries are updated imposing cell averages
					//flux_bc_flag is 1 if all the boundaries are updated with numerical fluxes
      int periodicX,periodicY,periodicZ;
	t_cell *cell;
	t_wall *wall;
  	// 	      ------(5)-----------(7)-----
	//          |		 |	         |
	//         (1)		(3)		  (4)
	//          |		 |	         |
	//          ------(0)-----------(2)-----
	t_node *node;
	//        (3)------------(4)------------(5)
	//         |		  |		     | 
	//         |		  |		     |
	//        (0)------------(1)------------(2)
	//
	
	double lambda_max;
	double tke;
	double mass,energy;

	t_sim *sim;
	
};

struct t_sim_{
	double dt,t,CFL; //dynamic variables
	double tf, tVolc; //static variables
	int rk_steps; //number of iterations of Runge-Kutta (1 if 1st order, 3 otherwise)
	int order; //order of accuracy
	int nvar; //number of variables

};


struct t_solid_{
	int nsolid;
	char * filename[50];
	t_stl *stl;
	
};

struct t_stl_{
	int ntri;
      int nver;
	char name[256];
      double Xmin[3],Xmax[3];
      int imin[3],imax[3]; 
	t_triangle *triangle;
	
};

struct t_triangle_{
      int outside; // 1 if at least one node is outside of the domain
	double nr[3],absnr;
	double p1[3],p2[3],p3[3];   
      int imin[3],imax[3]; 
};


////////////////////////////////////////////////////
//////  F U N C T I O N   D E F I N I T I O N //////
////////////////////////////////////////////////////


int create_mesh(t_mesh *mesh,t_sim *sim);
int update_initial(t_mesh *mesh, t_sim *sim);
int update_flux_boundaries(t_mesh *mesh);
int update_cell_boundaries(t_mesh *mesh);
int update_dt(t_mesh *mesh,t_sim *sim);

int assign_wall_type(t_mesh *mesh);
int assign_cell_type(t_mesh *mesh,t_solid *solids);
int assign_image_cells(t_mesh *mesh,t_solid *solids);
int update_ghost_cells(t_sim *sim,t_mesh *mesh,t_solid *solids);
int update_wall_type(t_mesh *mesh,t_solid *solids);
int update_stencils(t_mesh *mesh,t_sim *sim);

int write_vtk(t_mesh *mesh,char *filename);
int write_matrix(t_mesh *mesh,char *filename);
int write_matrix_u(t_mesh *mesh,char *filename);
int write_matrix_v(t_mesh *mesh,char *filename);
int write_list(t_mesh *mesh,char *filename);
int write_list_eq(t_mesh *mesh,char *filename);

void print_info(t_mesh *mesh, t_sim *sim);


double weno3L(double *phi);
double weno3R(double *phi);
double weno5L(double *phi);
double weno5R(double *phi);
double weno7L(double *phi);
double weno7R(double *phi);

void compute_solver(t_wall *wall);
void compute_transport(t_wall *wall);
void set_velocity_field(t_mesh *mesh, t_sim *sim);
void set_velocity(t_mesh *mesh, t_sim *sim);

void update_cell(t_mesh *mesh, t_sim *sim);

void update_cellK1(t_mesh *mesh, t_sim *sim);
void update_cellK2(t_mesh *mesh, t_sim *sim);
void update_cellK3(t_mesh *mesh, t_sim *sim);

int compute_fluxes(t_mesh *mesh, t_sim *sim);
int equilibrium_reconstruction(t_mesh *mesh, t_sim *sim);
void compute_source(t_mesh *mesh);

void compute_euler_HLLE(t_wall *wall,double *lambda_max);
void compute_euler_HLLC(t_wall *wall,double *lambda_max);
void compute_euler_HLLS(t_wall *wall,double *lambda_max, t_sim *sim);
void compute_transmissive_euler(t_wall *wall, int wp);
void compute_solid_euler_hlle(t_wall *wall, double *lambda_max, int wp);
void compute_euler_Roe(t_wall *wall,double *lambda_max);
void compute_sw_HLLE(t_wall *wall,double *lambda_max);
void compute_sw_Roe(t_wall *wall,double *lambda_max);
void compute_burgers_flux(t_wall *wall,double *lambda_max);
void compute_linear_flux(t_wall *wall,double *lambda_max);

void mass_calculation(t_mesh *mesh, t_sim *sim);
void energy_calculation(t_mesh *mesh, t_sim *sim);
void tke_calculation(t_mesh *mesh, t_sim *sim);

int read_solids(t_mesh *mesh, t_solid *solids );

int vector_product(double *input1, double *input2, double *output);
double dot_product(double *input1, double *input2);

double energy_from_pressure(double gm, double p, double u, double v, double w, double rho, double z);
double pressure_from_energy(double gm, double E, double u, double v, double w, double rho, double z);

////////////////////////////////////////////////////
//////  P R E - P R O C.   F U N C T I O N S ///////
////////////////////////////////////////////////////

int create_mesh(t_mesh *mesh, t_sim *sim){
	int i,l,m,n,k,aux,p,k2d;
	int xcells,ycells,zcells; 
	t_cell *cell;
	t_wall *wall;
	t_node *node;

	//int semiSt; //auxiliar variable to recalculate boundary cells stencil
	mesh->tke=0.0;


	mesh->sim=sim;

	//Cells
	xcells=mesh->xcells;
	ycells=mesh->ycells;
      zcells=mesh->zcells;
	mesh->ncells=xcells*ycells*zcells;
	mesh->cell=(t_cell*)malloc(mesh->ncells*sizeof(t_cell));
	cell=mesh->cell;
      
      for(n=0;n<zcells;n++){
            for(m=0;m<ycells;m++){
                  for(l=0;l<xcells;l++){
                        
                        k= xcells*m + l + n*xcells*ycells;
                        cell[k].id=999;
                        cell[k].l=l;
                        cell[k].m=m;
                        cell[k].n=n;


                        cell[k].w1_id=999;
                        cell[k].w2_id=999;
                        cell[k].w3_id=999;
                        cell[k].w4_id=999;
                        cell[k].w5_id=999;
                        cell[k].w6_id=999;

                        cell[k].dx=mesh->dx;
                        cell[k].dy=mesh->dy;
                        cell[k].dz=mesh->dz;
                        
                        cell[k].xc=999;
                        cell[k].yc=999;
                        cell[k].zc=999;
						
				cell[k].xim = 999; 
				cell[k].yim = 999;
				cell[k].zim = 999;
                        
                        cell[k].distabs = 9999999999999.0; 
                        
                        cell[k].distsolx = 9999999; 
                        cell[k].distsoly = 9999999; 
                        cell[k].distsolz = 9999999; 
                        
                        cell[k].out = 0;

                        cell[k].n1=999;
                        cell[k].n2=999;
                        cell[k].n3=999;
                        cell[k].n4=999;
                        cell[k].n5=999;
                        cell[k].n6=999;
                        cell[k].n7=999;
                        cell[k].n8=999;
                  }
            }
      }
      
	//Walls
	mesh->nwalls=3*mesh->ncells+xcells*zcells+ycells*zcells+xcells*ycells;
	mesh->wall=(t_wall*)malloc(mesh->nwalls*sizeof(t_wall));

	wall=mesh->wall;
	for(k=0;k<mesh->nwalls;k++){
		wall[k].id=k;
		wall[k].fR_star=(double*)malloc(sim->nvar*sizeof(double));
		wall[k].fL_star=(double*)malloc(sim->nvar*sizeof(double));
		wall[k].UR=(double*)malloc(sim->nvar*sizeof(double));
		wall[k].UL=(double*)malloc(sim->nvar*sizeof(double));
            wall[k].URe=(double*)malloc(sim->nvar*sizeof(double));
		wall[k].ULe=(double*)malloc(sim->nvar*sizeof(double));
	}

	//Walls amd nodes of the cells
      for(n=0;n<zcells;n++){
            for(m=0;m<ycells;m++){
                  for(l=0;l<xcells;l++){
                        
                        k = l + m*xcells + n*xcells*ycells;
                        k2d=l + m*xcells;
                        cell[k].id=k;
                        cell[k].l=l;
                        cell[k].m=m;
                        

                        cell[k].w1_id=3*(k2d)+m+n*(3*xcells*ycells+xcells+ycells);
                        cell[k].w4_id=cell[k].w1_id+1;
                        cell[k].w5_id=cell[k].w1_id+2;
                        if (l==xcells-1){
                              cell[k].w2_id=cell[k].w1_id+4 - 1;
                        }else{
                              cell[k].w2_id=cell[k].w1_id+4;
                        }
                        if (m==ycells-1){
                              aux=(3*xcells*ycells+xcells+ycells)*(n+1);
                              cell[k].w3_id=aux-xcells+l; 
                        }else{
                              cell[k].w3_id=cell[k].w1_id+xcells*3+1;
                        }
                        if (n==zcells-1){
                              cell[k].w6_id=mesh->nwalls-xcells*ycells+l+m*xcells;
                        }else{
                              cell[k].w6_id=cell[k].w5_id+(3*xcells*ycells+xcells+ycells);
                        }

                        cell[k].w1=&(mesh->wall[cell[k].w1_id]);
                        cell[k].w2=&(mesh->wall[cell[k].w2_id]);
                        cell[k].w3=&(mesh->wall[cell[k].w3_id]);
                        cell[k].w4=&(mesh->wall[cell[k].w4_id]);
                        cell[k].w5=&(mesh->wall[cell[k].w5_id]);
                        cell[k].w6=&(mesh->wall[cell[k].w6_id]);

                        cell[k].dx=mesh->dx;
                        cell[k].dy=mesh->dy;
                        cell[k].dz=mesh->dz;
                        
                        cell[k].xc=(l+0.5)*cell[k].dx;
                        cell[k].yc=(m+0.5)*cell[k].dy;
                        cell[k].zc=(n+0.5)*cell[k].dz;
                        
                        cell[k].n1=k2d +m +n*(xcells+1)*(ycells+1);
                        cell[k].n2=cell[k].n1+1;
                        cell[k].n3=cell[k].n2+xcells+1;
                        cell[k].n4=cell[k].n2+xcells;
                        cell[k].n5=cell[k].n1 + (xcells+1)*(ycells+1);
                        cell[k].n6=cell[k].n5+1;
                        cell[k].n7=cell[k].n6+xcells+1;
                        cell[k].n8=cell[k].n6+xcells;

                  }
            }
      }
      
	
	//normal vectors of the walls	
  	for(k=0;k<mesh->ncells;k++){
		mesh->wall[cell[k].w1_id].nx=0.0;
		mesh->wall[cell[k].w4_id].nx=1.0;
		mesh->wall[cell[k].w1_id].ny=1.0;
		mesh->wall[cell[k].w4_id].ny=0.0;
            mesh->wall[cell[k].w1_id].nz=0.0;
		mesh->wall[cell[k].w4_id].nz=0.0;

		//we know this is redundant,
		//but it's easier and is pre-process
		mesh->wall[cell[k].w3_id].nx=0.0;
		mesh->wall[cell[k].w2_id].nx=1.0;
		mesh->wall[cell[k].w3_id].ny=1.0;
		mesh->wall[cell[k].w2_id].ny=0.0;
            mesh->wall[cell[k].w3_id].nz=0.0;
		mesh->wall[cell[k].w2_id].nz=0.0;
            
            mesh->wall[cell[k].w5_id].nx=0.0;
		mesh->wall[cell[k].w6_id].nx=0.0;
		mesh->wall[cell[k].w5_id].ny=0.0;
		mesh->wall[cell[k].w6_id].ny=0.0;
            mesh->wall[cell[k].w5_id].nz=1.0;
		mesh->wall[cell[k].w6_id].nz=1.0;

	}
		

      
	//Nodes
	mesh->nnodes=(mesh->xcells+1)*(mesh->ycells+1)*(mesh->zcells+1);
	mesh->node=(t_node*)malloc(mesh->nnodes*sizeof(t_node));
	
	node=mesh->node;
	//Loop for nodes
      for(n=0;n<zcells+1;n++){
            for(m=0;m<ycells+1;m++){
                  for(l=0;l<xcells+1;l++){
                        k=(xcells+1)*(ycells+1)*n+(xcells+1)*m+l;
                        node[k].id=k;
                        node[k].x=l*mesh->dx;
                        node[k].y=m*mesh->dy;
                        node[k].z=n*mesh->dz;
                  }
            }
      }
      
	//Boundary condition flags
	//This is for BC imposition	
	if(mesh->bc[0]==99 && mesh->bc[1]==99 && mesh->bc[2]==99 && mesh->bc[3]==99){ //this has been temporary deactivated
		//all the boundaries are updated imposing cell averages
		mesh->cell_bc_flag=1; 
 	}else{
		mesh->cell_bc_flag=0;
	}
	if(mesh->bc[0]!=1 || mesh->bc[1]!=1 || mesh->bc[2]!=1 || mesh->bc[3]!=1 || mesh->bc[4]!=1 || mesh->bc[5]!=1){
		//all the boundaries are updated with fluxes
		mesh->flux_bc_flag=1; 
 	}else{
		mesh->flux_bc_flag=0;
	}

	//This defines boundaries for interpolation
	if(mesh->bc[1]==1 && mesh->bc[3]==1){
		mesh->periodicX=1;
	}else{
		mesh->periodicX=0;
	}
	if(mesh->bc[0]==1 && mesh->bc[2]==1){
		mesh->periodicY=1;
	}else{
		mesh->periodicY=0;
	}
      if(mesh->bc[4]==1 && mesh->bc[5]==1){
		mesh->periodicZ=1;
	}else{
		mesh->periodicZ=0;
	}

      
	//Assigment of wall's neighbour cells 
      for(n=0;n<zcells;n++){
            for(m=0;m<ycells;m++){
                  for(l=0;l<xcells;l++){
                        
                        //Interior walls
                        k = l + m*xcells + n*xcells*ycells;
                        
                  //	mesh->wall[cell[k].w1_id].cellR_id=cell[k].id;
                        cell[k].w1->cellR_id=cell[k].id;
                        //mesh->wall[cell[k].w4_id].cellR_id=cell[k].id;
                        cell[k].w4->cellR_id=cell[k].id;
                        cell[k].w5->cellR_id=cell[k].id;

                        //mesh->wall[cell[k].w2_id].cellL_id=cell[k].id;
                        cell[k].w2->cellL_id=cell[k].id;
                        //mesh->wall[cell[k].w3_id].cellL_id=cell[k].id;
                        cell[k].w3->cellL_id=cell[k].id;
                        cell[k].w6->cellL_id=cell[k].id;

                        cell[k].w1->cellR=&(cell[k]);
                        cell[k].w4->cellR=&(cell[k]);
                        cell[k].w5->cellR=&(cell[k]);
                        cell[k].w2->cellL=&(cell[k]);
                        cell[k].w3->cellL=&(cell[k]);
                        cell[k].w6->cellL=&(cell[k]);

                                          //Boundary walls need to stablish their external cells
                        if(m==0){
                              //If we are at the first row position (m=0)
                              //the left cell of the boundary walls are the last row cells
                              //assuming periodic boundaries (if not, the fluxes will be rewritten later on
                              //mesh->wall[cell[k].w1_id].cellL_id=cell[xcells*(ycells-1)+l].id;
                              cell[k].w1->cellL_id=cell[k+(ycells-1)*xcells].id;  //xcells*(ycells-1)+l+n*zcell
                              cell[k].w1->cellL=&(cell[k+(ycells-1)*xcells]);
                        }
                        
                        if(m==ycells-1){
                              //If we are at the last row position (m=ycells-1)
                              //the right cell of the boundary walls are the first row cells
                              //assuming periodic boundaries (if not, the fluxes will be rewritten later on
                              //mesh->wall[cell[k].w3_id].cellR_id=cell[l].id;
                              cell[k].w3->cellR_id=cell[k-m*xcells].id;
                              cell[k].w3->cellR=&(cell[k-m*xcells]);
                        }

                        if(l==0){
                              //mesh->wall[cell[k].w4_id].cellL_id=cell[(m+1)*xcells-1].id;		
                              cell[k].w4->cellL_id=cell[k+(xcells-1)].id;		
                              cell[k].w4->cellL=&(cell[k+(xcells-1)]);		
                        }
                        if(l==xcells-1){
                              //mesh->wall[cell[k].w2_id].cellR_id=cell[m*xcells].id;		
                              cell[k].w2->cellR_id=cell[k-(xcells-1)].id;		
                              cell[k].w2->cellR=&(cell[k-(xcells-1)]);		
                        }
                        
                        if(n==0){
                              //mesh->wall[cell[k].w4_id].cellL_id=cell[(m+1)*xcells-1].id;		
                              cell[k].w5->cellL_id=cell[k+(zcells-1)*xcells*ycells].id;		
                              cell[k].w5->cellL=&(cell[k+(zcells-1)*xcells*ycells]);		
                        }
                        if(n==zcells-1){
                              //mesh->wall[cell[k].w2_id].cellR_id=cell[m*xcells].id;		
                              cell[k].w6->cellR_id=cell[k-(zcells-1)*xcells*ycells].id;		
                              cell[k].w6->cellR=&(cell[k-(zcells-1)*xcells*ycells]);		
                        }

                  }
            }
      }
	//Allocation of arrays of variables in cells and walls


	for(k=0;k<mesh->ncells;k++){		
		mesh->cell[k].U=    (double*)malloc(sim->nvar*sizeof(double));
		mesh->cell[k].U_aux=(double*)malloc(sim->nvar*sizeof(double));
		mesh->cell[k].Ue   =(double*)malloc(sim->nvar*sizeof(double));
		mesh->cell[k].S=    (double*)malloc(sim->nvar*sizeof(double));
            mesh->cell[k].S_corr=    (double*)malloc(sim->nvar*sizeof(double));
	}
	
	for(k=0;k<mesh->nwalls;k++){
		mesh->wall[k].UR=(double*)malloc(sim->nvar*sizeof(double));
		mesh->wall[k].UL=(double*)malloc(sim->nvar*sizeof(double));
	}
	
	cell=mesh->cell;
	for(n=0;n<mesh->ncells;n++){	
		for(k=0;k<sim->nvar;k++){	
			cell[n].S[k]=0.0 ;
                  cell[n].S_corr[k]=0.0 ;
		}
	}
	
	for(n=0;n<mesh->ncells;n++){		
		cell[n].w1->z=cell[n].zc;
		cell[n].w4->z=cell[n].zc;
		cell[n].w2->z=cell[n].zc;
		cell[n].w3->z=cell[n].zc;
		cell[n].w5->z=cell[n].zc-0.5*cell[n].dz;
		cell[n].w6->z=cell[n].zc+0.5*cell[n].dz;
	}
	


	
	return 1;
}


int update_stencils(t_mesh *mesh,t_sim *sim){
	int i,l,m,n,k,aux,p,k2d;
	int xcells,ycells,zcells; 
	t_cell *cell;

	int semiSt; //auxiliar variable to recalculate boundary cells stencil
	
	//Cells
	xcells=mesh->xcells;
	ycells=mesh->ycells;
      zcells=mesh->zcells;
	cell=mesh->cell;

      
      //Set cell stencils
	//Initially all the cells has a stencil of size order
	for(k=0;k<mesh->ncells;k++){
		cell[k].st_sizeX=sim->order;
		cell[k].st_sizeY=sim->order;
            cell[k].st_sizeZ=sim->order;
	}

	//But there are special cases at boundary cells	
	semiSt=(sim->order-1)*0.5;
      for(n=0;n<zcells;n++){
            for(m=0;m<ycells;m++){
                  for(l=0;l<xcells;l++){
                        k = l + m*xcells + n*xcells*ycells;
                        k2d=l + m*xcells;
                        if(mesh->periodicX==0){
                              //x setencils
                              if(l<semiSt){
                                    cell[k].st_sizeX=MIN(cell[k].st_sizeX,2*l+1); //poner if elseif else y añadir en todas min(st_size,dist*2+1)
                              }else if(xcells-(l+1)<semiSt){
                                    cell[k].st_sizeX=MIN(cell[k].st_sizeX,2*(xcells-(l+1))+1);
                              }
                              cell[k].st_sizeX=MIN(cell[k].st_sizeX,2*cell[k].distsolx-1);
                              
                        }
                        if(mesh->periodicY==0){
                              //y stencils
                              if(m<semiSt){
                                    cell[k].st_sizeY=MIN(cell[k].st_sizeY,2*m+1);
                              }else if(ycells-(m+1)<semiSt){
                                    cell[k].st_sizeY=MIN(cell[k].st_sizeY,2*(ycells-(m+1))+1);
                              }
                              cell[k].st_sizeY=MIN(cell[k].st_sizeY,2*cell[k].distsoly-1);
                        }
                        if(mesh->periodicZ==0){
                              //y stencils
                              if(n<semiSt){
                                    cell[k].st_sizeZ=MIN(cell[k].st_sizeZ,2*n+1);
                              }else if(zcells-(n+1)<semiSt){
                                    cell[k].st_sizeZ=MIN(cell[k].st_sizeZ,2*(zcells-(n+1))+1);
                              }
                              cell[k].st_sizeZ=MIN(cell[k].st_sizeZ,2*cell[k].distsolz-1);
                        }
                  }
            }
	}
	//Initialization of cell stencils
	for(k=0;k<mesh->ncells;k++){
		for(p=0;p<9;p++){
			cell[k].stX[p]=-1;
			cell[k].stY[p]=-1;
                  cell[k].stZ[p]=-1;
		}
	}
	
      for(n=0;n<zcells;n++){
            for(m=0;m<ycells;m++){
                  for(l=0;l<xcells;l++){
                        
                        k = l + m*xcells + n*xcells*ycells;
                        k2d=l + m*xcells;
                        
                        //x setencils
                        for(p=0;p<cell[k].st_sizeX;p++){
                              if(mesh->periodicX==0){
                                    //nunca nos salimos de la malla
                                    cell[k].stX[p]=l-((cell[k].st_sizeX-1)/2)+p;
                              }else{
                                    //aux=l-((cell[k].st_sizeX-1)*0.5)+p;
                                    cell[k].stX[p]=l-((cell[k].st_sizeX-1)/2)+p;
                                    if(cell[k].stX[p]<0){
                                          cell[k].stX[p]+=xcells*(1);
                                    }
                                    if(cell[k].stX[p]>xcells-1){
                                          cell[k].stX[p]+=xcells*(-1);

                                    }

                              }
                              cell[k].stX[p]+=xcells*m;
                              cell[k].stX[p]+=xcells*ycells*n;
                        }
                       
                        
                        //y stencils 
                        for(p=0;p<cell[k].st_sizeY;p++){
                              if(mesh->periodicY==0){
                                    //nunca nos salimos de la malla
                                    cell[k].stY[p]=m-((cell[k].st_sizeY-1)/2)+p;
                              }else{
                                    //aux=l-((cell[k].st_sizeX-1)*0.5)+p;
                                    cell[k].stY[p]=m-((cell[k].st_sizeY-1)/2)+p;
                                    if(cell[k].stY[p]<0){
                                          cell[k].stY[p]+=ycells*(1);
                                    }
                                    if(cell[k].stY[p]>ycells-1){
                                          cell[k].stY[p]+=ycells*(-1);

                                    }

                              }
                              cell[k].stY[p]=xcells*cell[k].stY[p]+l;
                              cell[k].stY[p]+=xcells*ycells*n;
                        }
                        
                        //z stencils 
                        for(p=0;p<cell[k].st_sizeZ;p++){
                              if(mesh->periodicZ==0){
                                    //nunca nos salimos de la malla
                                    cell[k].stZ[p]=n-((cell[k].st_sizeZ-1)/2)+p;
                              }else{
                                    //aux=l-((cell[k].st_sizeX-1)*0.5)+p;
                                    cell[k].stZ[p]=n-((cell[k].st_sizeZ-1)/2)+p;
                                    if(cell[k].stZ[p]<0){
                                          cell[k].stZ[p]+=zcells*(1);
                                    }
                                    if(cell[k].stZ[p]>zcells-1){
                                          cell[k].stZ[p]+=zcells*(-1);

                                    }

                              }
                              cell[k].stZ[p]=ycells*xcells*cell[k].stZ[p]+k2d;
                        }
                                    
                  
                  
                  
                  }
            }
	   
      }
      
      
      return 1;

}






int update_initial(t_mesh *mesh, t_sim *sim){
	int l,m,k,n,RP;
	FILE *fp,*fpe;
	char fname[1024];
      double d,aux1,aux2,aux3,xaux,yaux,r,umax,ut,wp,r_d,L,xc,yc,zc,rc,d1,d2;
      double x1,x2,y1,y2,z1,z2,C;
      double p,u,v,w,rho,phi,gamma,tt,p0,tt0,rho0,pexner,T0,BV;
	  
	t_cell *cell;
	cell=mesh->cell;
	
	sprintf(fname,"case/initial.out");
	fp = fopen(fname,"r");
	
	sprintf(fname,"case/equilibrium.out");
	fpe = fopen(fname,"r");

#if READ_INITIAL	
	if (fpe != NULL){
		
	}else{
		printf("%s File case/equilibrium.out not found. Equilibrium is set as default. \n",WAR);
		for(k=0;k<mesh->ncells;k++){
			cell[k].prese=0.0;	
			for(m=0;m<sim->nvar;m++){
				cell[k].Ue[m]=0.0;
			}
		}
	}

	if (fp != NULL){
		fscanf(fp, "%*[^\n]\n");
		fscanf(fp, "%*[^\n]\n");
		
		for(l=0;l<mesh->xcells;l++){  
			for(m=0;m<mesh->ycells;m++){
				for(n=0;n<mesh->zcells;n++){
				k = l + m*mesh->xcells + n*mesh->xcells*mesh->ycells;
				fscanf(fp,"%*f %*f %*f %le %le %le %le %le %le",&u,&v,&w,&rho,&p,&phi);
				//printf("%14.14e %14.14e %14.14e %14.14e %14.14e \n",u,v,w,rho,p);
				#if MULTICOMPONENT
					#if MULTI_TYPE==1
					gamma=phi;
					#else
					gamma=1.0+1.0/phi;
					#endif
				#else
					gamma=_gamma_;
				#endif
				cell[k].U[0]=rho;
				cell[k].U[1]=u*cell[k].U[0];
				cell[k].U[2]=v*cell[k].U[0];
				cell[k].U[3]=w*cell[k].U[0];
				cell[k].U[4]=energy_from_pressure(gamma,p,u,v,w,rho,cell[k].zc);
				cell[k].U[5]=phi*rho;
				}
			}
		}
		
		fclose(fp);
		
		printf("%s Initial state file has been read \n",OK);
		
	}else{

	printf("%s File case/initial.out not found. Initial data is set in update_initial() \n",WAR);
#else
	printf("%s Read initial data from file case/initial.out is disabled \n",WAR);
#endif

#if EULER && !LINEAR_TRANSPORT

      L=mesh->Lx;//1.0;
      for(k=0;k<mesh->ncells;k++){
      

            if(cell[k].type==1){
                  
                  u=0.0;
                  v=0.0;
                  w=0.0;
                  phi=0.0;
                  
                  
                  //r=sqrt((cell[k].xc-xc)*(cell[k].xc-xc)+(cell[k].yc-yc)*(cell[k].yc-yc)+(cell[k].zc-zc)*(cell[k].zc-zc));
                  //r=sqrt((cell[k].xc-xc)*(cell[k].xc-xc)+(cell[k].yc-yc)*(cell[k].yc-yc));


                  x1=cell[k].zc-cell[k].dz/2;
                  x2=cell[k].zc+cell[k].dz/2;
                  
                  tt0=300;
                  p0=_p0_; 
                  rho0=p0/(_R_*tt0);
                  
                  tt=tt0;
                  //aux1=p0*_R_*tt*_gamma_/(_g_*(2.0*_gamma_-1.0));
                  aux2=(_gamma_-1.0)/_gamma_*_g_/(_R_*tt);
                  //aux3=(2.0*_gamma_-1.0)/(_gamma_-1.0);
                  
			//p= -aux1*( pow((1.0-aux2*x2),aux3) - pow((1.0-aux2*x1),aux3)  )/cell[k].dz;  //integrado

                  //aux3=_gamma_/(_gamma_-1.0);
                  
                  //rho=  -p0/_g_*( pow((1.0-aux2*x2),aux3) - pow((1.0-aux2*x1),aux3)  )/cell[k].dz;
                  
                  p=   p0*  pow((1.0-aux2*cell[k].zc),_gamma_/(_gamma_-1.0));  //pointwise
                  rho= rho0*pow((1.0-aux2*cell[k].zc),1.0/(_gamma_-1.0));
                  
               
			cell[k].Ue[0]=rho;
			cell[k].Ue[1]=u*cell[k].Ue[0];
			cell[k].Ue[2]=v*cell[k].Ue[0];
			cell[k].Ue[3]=w*cell[k].Ue[0];
			cell[k].Ue[4]=energy_from_pressure(_gamma_,p,u,v,w,rho,cell[k].zc);
			cell[k].Ue[5]=phi;
                  
                  cell[k].prese=p;
			
			for(m=0;m<sim->nvar;m++){
				cell[k].U[m]=cell[k].Ue[m];
			}
			             

                  xc=10000;
                  zc=2000;
                  d1=sqrt((cell[k].xc-xc)*(cell[k].xc-xc)+(cell[k].zc-zc)*(cell[k].zc-zc));   
                  
                  xc=10000;
                  zc=8000;
                  d2=sqrt((cell[k].xc-xc)*(cell[k].xc-xc)+(cell[k].zc-zc)*(cell[k].zc-zc));
                  
                  rc=1000;
                  
                  aux1=20.0*(MAX(rc-d1/2.0,0.0)+MIN(d2/2.0-rc,0.0))/1000;
                  
                                   
                  tt=tt0+aux1;
                  aux2=(_gamma_-1.0)/_gamma_*_g_/(_R_*tt0);
                  p=   p0*  pow((1.0-aux2*cell[k].zc),_gamma_/(_gamma_-1.0));  //pointwise
                  rho= p0/(_R_*tt)*pow((1.0-aux2*cell[k].zc),1.0/(_gamma_-1.0));
			u=0.0;
                  v=0.0;
                  w=0.0;
                  phi=0.0;
                  
			cell[k].U[0]=rho;
			cell[k].U[1]=u*cell[k].U[0];
			cell[k].U[2]=v*cell[k].U[0];
			cell[k].U[3]=w*cell[k].U[0];
			cell[k].U[4]=energy_from_pressure(_gamma_,p,u,v,w,rho,cell[k].zc);
			cell[k].U[5]=phi;
			
                  
            }else{
                  
                  cell[k].Ue[0]=-1.0;
                  cell[k].Ue[1]=0.0;
                  cell[k].Ue[2]=0.0;
                  cell[k].Ue[3]=0.0;
                  cell[k].Ue[4]=0.0;
                  cell[k].Ue[5]=0.0;
                  
			for(m=0;m<sim->nvar;m++){
				cell[k].U[m]=cell[k].Ue[m];
			}
                 
                  
            }
            
      } 

#if READ_INITIAL		
	}
#endif

#endif


#if LINEAR_TRANSPORT		
for(k=0;k<mesh->ncells;k++){
      
            u=mesh->u_x;
            v=mesh->u_y;
            w=mesh->u_z;
            
            xc=0.5;
            yc=0.5;
            zc=0.5;
            
            x1=cell[k].xc-mesh->dx/2.0;
            x2=cell[k].xc+mesh->dx/2.0;
            y1=cell[k].yc-mesh->dy/2.0;
            y2=cell[k].yc+mesh->dy/2.0;
            
            L=mesh->Lx;
            
            // this is a simple sphere
            r=sqrt((cell[k].xc-xc)*(cell[k].xc-xc)+(cell[k].yc-yc)*(cell[k].yc-yc)+(cell[k].zc-zc)*(cell[k].zc-zc));
            //r=sqrt((cell[k].xc-xc)*(cell[k].xc-xc)+(cell[k].yc-yc)*(cell[k].yc-yc));
            if (r<0.25) {
                  phi=0.0;
            }else{
                  phi=0.0;
            }
            
            // the following function is implemented: 1/(dx*dy)* int_x1^x2 int_y1^y2 [sin(2*pi/L*(x+y))] dy dx
            //C=2.0*PI/L;          
            //phi=1.0/(C*C)*( -sin(C*(x2+y2)) + sin(C*(x1+y2)) + sin(C*(x2+y1)) - sin(C*(x1+y1)) ) / (mesh->dx*mesh->dy);
            //printf("Celda %d con phi %lf \n", k, phi);   
            
            
            //NO TOCAR:
            cell[k].U[0]=1.0;
            cell[k].U[1]=u*cell[k].U[0];
            cell[k].U[2]=v*cell[k].U[0];
            cell[k].U[3]=w*cell[k].U[0];
            cell[k].U[4]=1.0;
            cell[k].U[5]=phi;

	}

#endif


#if BURGERS
for(k=0;k<mesh->ncells;k++){
      
      xc=0.5;
      yc=0.5;
      zc=0.5;
      r=sqrt((cell[k].xc-xc)*(cell[k].xc-xc)+(cell[k].yc-yc)*(cell[k].yc-yc)+(cell[k].zc-zc)*(cell[k].zc-zc));
      
      cell[k].U[0]=1.0*r;
      
}

#endif

#if LINEAR
for(k=0;k<mesh->ncells;k++){
      
      xc=0.5;
      yc=0.5;
      zc=0.5;
      r=sqrt((cell[k].xc-xc)*(cell[k].xc-xc)+(cell[k].yc-yc)*(cell[k].yc-yc)+(cell[k].zc-zc)*(cell[k].zc-zc));
      
              
      if (r<0.25) {
            cell[k].U[0]=1.0;
      }else{
            cell[k].U[0]=2.0;
      }
      
}

#endif


	return 1;
}


int update_cell_boundaries(t_mesh *mesh){
      int l,m,k,n;
      double p,u,v,w,rho,phi,gamma,zc;
      t_cell *cell;
	cell=mesh->cell;
      rho=1.0 ; 
      u=0.0;
      v=0.0;
      w=0.0;
      
	/*
      for(k=0;k<mesh->ncells;k++){
      
                  p=-1.0/cell[k].dz*(exp(-(cell[k].zc+cell[k].dz/2)) - exp(-(cell[k].zc-cell[k].dz/2)));
			rho=p;
                  
                  if(cell[k].n<1 ){
  
                        cell[k].U[0]=rho;

                        cell[k].U[4]=p/(_gamma_-1.0)+0.5*rho*(u*u + v*v + w*w); // si la quitamos, error 1.e-12
                        
                  }
                  
                  if(cell[k].n> mesh->zcells-2){


                        cell[k].U[4]=p/(_gamma_-1.0)+0.5*rho*(u*u + v*v + w*w);
                        
                  
                  
      }
	}*/

	return 1;
}




int assign_cell_type(t_mesh *mesh,t_solid *solids){ // Define ghost and solid cells
	t_cell *cell;
	int m,l,n,k,i,j,q,i1,i2,i3,na,df,df0,ct;
      double aux1,aux2;
      double r;
      double xci,yci,zci,proj,dist,s1,s2,s3;
      double dif[3],xc[3],v1[3],v2[3],v3[3],vp1[3],vp2[3],vp3[3],dc1[3],dc2[3],dc3[3];
      double dp;
      int solx,soly,solz;
      t_triangle *triangle;
      
      cell=mesh->cell;
                  
      for(k=0;k<mesh->ncells;k++){
            cell[k].type=1;          //by default 1.    1= computed cell, 0= solid cell (not computed cell)
            cell[k].ghost=0;		
      }
      
#if ALLOW_SOLIDS==1
      /*
      //solid cells are assigned using simple formulas. In the future, "find-point-inside" algorithms will be used.
      */
      
      if(solids->nsolid<1){
            printf("%s In function assign_cell_type() no solids are considered\n",WAR);
      }else{
      
            for(l=0;l<solids->nsolid;l++){ // loop for each solid
                  triangle=solids->stl[l].triangle;
                  //printf("Solid %d: boundary cells are being computed...\n\n",l);
                  for(m=0;m<solids->stl[l].ntri;m++){ // loop for each triangle
                        //if(triangle[m].outside==0){
                              for(q=0;q<3;q++){
                                    v1[q]=triangle[m].p2[q]-triangle[m].p1[q]; // the vectors between nodes are defined
                                    v2[q]=triangle[m].p3[q]-triangle[m].p2[q];
                                    v3[q]=triangle[m].p1[q]-triangle[m].p3[q];
                              }
                              dp=MAX(mesh->dx,mesh->dy);
                              dp=MAX(dp,mesh->dz);
                              dp=_stol_*dp ; //2.0*fabs(cell[n].dx*triangle[m].nr[0]+cell[n].dy*triangle[m].nr[1]+cell[n].dz*triangle[m].nr[2])/triangle[m].absnr;
                              //dp=_stol_*mesh->dx;
                              
                              //loop over cells inside the bounding box of the triangle
                              for(i=triangle[m].imin[0];i<=triangle[m].imax[0];i++){
                                    for(j=triangle[m].imin[1];j<=triangle[m].imax[1];j++){
                                          for(k=triangle[m].imin[2];k<=triangle[m].imax[2];k++){
                                                //printf("triangle %d, cell (%d %d %d) \n",m,i,j,k);
                                                n= mesh->xcells*j + i + k*mesh->xcells*mesh->ycells;
                                                dif[0]=cell[n].xc-triangle[m].p1[0]; //vector from cell center to node P1 (x-component)
                                                dif[1]=cell[n].yc-triangle[m].p1[1]; //vector from cell center to node P1 (y-component)
                                                dif[2]=cell[n].zc-triangle[m].p1[2]; //vector from cell center to node P1 (z-component)
                                                proj=dif[0]*triangle[m].nr[0]+dif[1]*triangle[m].nr[1]+dif[2]*triangle[m].nr[2]; //vector from cell center to triangle P1, projected onto normal direction
                                                dist=proj/triangle[m].absnr;  
                                                if (proj>0 && cell[n].ghost>0 && fabs(dist)<cell[n].distabs){ // if a cell has been tagged as "ghost" and is now found outside of other triangle, it is reverted as "fluid cell"
                                                      cell[n].ghost=0; 
                                                }
                                                if (proj<=0&&cell[n].out<1) { //when proj<0 and the cell is not already outside of other triangle, the cell center is inside the solid (below the surface)       
                                                      if (fabs(dist)<dp) { 
                                                            xc[0]=cell[n].xc - dist*triangle[m].nr[0]; //Formula: Xc = Xcell - proj * n / abs(n) allows to compute the intersection point on the triangle, xc
                                                            xc[1]=cell[n].yc - dist*triangle[m].nr[1];
                                                            xc[2]=cell[n].zc - dist*triangle[m].nr[2];
                                                            
                                                            // Below, the point-in-triangle algorithm (3D version) is used to detect if the intersection point, xc, is inside the triangle
                                                            for(q=0;q<3;q++){
                                                                  dc1[q]=xc[q]-triangle[m].p1[q];
                                                                  dc2[q]=xc[q]-triangle[m].p2[q];
                                                                  dc3[q]=xc[q]-triangle[m].p3[q];
                                                            }
                                                            
                                                            vector_product(v1,dc1,vp1); // the point-in-triangle algorithm  is based on cross products to determine whether point is inside or not
                                                            vector_product(v2,dc2,vp2);
                                                            vector_product(v3,dc3,vp3);
                                                            
                                                            s1=dot_product(vp1,vp2);
                                                            s2=dot_product(vp2,vp3);
                                                            s3=dot_product(vp3,vp1);
                                                           
                                                            if (s1>0 && s2>0 &&  s3>0) { // when all are positive (same vector product direction), the point xc is inside the triangle
                                                                  //cell[n].type=0;
                                                                  if(fabs(dist)<cell[n].distabs){  // the condition before may happen for many triangles, so we keep the closest to the interface                                              
                                                                        cell[n].ghost=1;
                                                                        cell[n].solid_id=l;
                                                                        cell[n].triangle_id=m;
                                                                        cell[n].tri=&(triangle[m]);
                                                                        if(fabs(dist)<TOL14){
                                                                              dist=-TOL14; //to ensure negativity of dist, that might be zero and therefore +-0
                                                                        }
                                                                        cell[n].distabs=fabs(dist);
                                                                        //printf("triangle %14.14e \n",dist);
                                                                        cell[n].xim = xc[0] - dist*triangle[m].nr[0]; 
                                                                        cell[n].yim = xc[1] - dist*triangle[m].nr[1];
                                                                        cell[n].zim = xc[2] - dist*triangle[m].nr[2];
                                                                  }
                                                            }
                                                      }
                                                
                                                }else{
                                                            cell[n].out=1;
                                                            
                                                }
                                    
                                          }
                                    }     
                              }
                              //getchar();
                        
                        //}
            
                  }
            }
            
            //printf("%s Ghost cells have been identified \n\n",OK);
            
            
            for(l=0;l<solids->nsolid;l++){ //loop over cells inside the solid bounding box
                  for(i=solids->stl[l].imin[0];i<=solids->stl[l].imax[0];i++){
                              for(j=solids->stl[l].imin[1];j<=solids->stl[l].imax[1];j++){
                                    for(k=solids->stl[l].imin[2];k<=solids->stl[l].imax[2];k++){
                                    
                                          n= mesh->xcells*j + i + k*mesh->xcells*mesh->ycells;
                                          if(cell[n].ghost!=1){
                                          
                                                solx=0;
                                                soly=0;
                                                solz=0;
                                                
                                                df0=mesh->xcells;
                                                q=-1;
                                                ct=0; // we define a counter and only if ct is greater or equal to 2, solids are defined, to prevent from individual ghost cells generating solid lines
                                                for (i1=0;i1<mesh->xcells;i1++){ //loop over lines to determine the closest interface to a point, in x-direction
                                                      na=mesh->xcells*j + i1 + k*mesh->xcells*mesh->ycells;
                                                      if(cell[na].ghost==1){
                                                            ct+=1; //counter to check the number of ghost cells in a line
                                                            df=abs(i1-i); //distance from i1 to i
                                                            if(df<=df0){
                                                                  df0=df;
                                                                  q=na; //this is the index of the nearest ghost cell to cell n
                                                            }                                                     
                                                      }                                                
                                                }
                                                if(q>-1&&ct>1){
                                                      aux1=cell[q].xc-cell[n].xc;
                                                      aux2=aux1*cell[q].tri->nr[0];
                                                      if(aux2>0.0){ //when the vector points out the surface, the cell is inside
                                                            solx=1;//cell[n].type=0;        
                                                      }
                                                }
                                                
                                                //if(cell[n].xc>0.75&&cell[n].type==0){
                                                //      printf("cell %d: q is %d, located at x=%lf \n",n,q,cell[q].xc);
                                                //      printf("ny=%lf \n",cell[q].tri->nr[0]);
                                                //      getchar();
                                                //}
                                                
                                                //same than above, for "y" 
                                                if(cell[n].type!=0){
                                                df0=mesh->ycells;
                                                q=-1;
                                                ct=0;
                                                for (i2=0;i2<mesh->ycells;i2++){
                                                      na=mesh->xcells*i2 + i + k*mesh->xcells*mesh->ycells;
                                                      if(cell[na].ghost==1){
                                                            ct+=1;
                                                            df=abs(i2-j);
                                                            if(df<=df0){
                                                                  df0=df;
                                                                  q=na;
                                                            }                                                     
                                                      }                                                
                                                }
                                                if(q>-1&&ct>1){
                                                      aux1=cell[q].yc-cell[n].yc;
                                                      aux2=aux1*cell[q].tri->nr[1];
                                                      if(aux2>0.0){
                                                            soly=1;//cell[n].type=0;     
                                                      }
                                                }
                                                }
                                                
                                                //same than above, for "z"
                                                if(cell[n].type!=0){
                                                df0=mesh->zcells;
                                                q=-1;
                                                ct=0;
                                                for (i3=0;i3<mesh->zcells;i3++){
                                                      na=mesh->xcells*j + i + i3*mesh->xcells*mesh->ycells;
                                                      if(cell[na].ghost==1){
                                                            ct+=1;
                                                            df=abs(i3-k);
                                                            if(df<=df0){
                                                                  df0=df;
                                                                  q=na;
                                                            }                                                     
                                                      }                                                
                                                }
                                                if(q>-1&&ct>1){
                                                      aux1=cell[q].zc-cell[n].zc;
                                                      aux2=aux1*cell[q].tri->nr[2];
                                                      if(aux2>0.0){
                                                            solz=1;//cell[n].type=0;     
                                                      }
                                                }
                                                }
                                                
                                                if(solx==1&&soly==1){ //when intersections are detected in X and Y ray tracing, then the cell is set as solid.
                                                      cell[n].type=0; 
                                                }
                                          
                                          
                                          }
                                          
                                    }
                                    
                              }
                              
                  }
                  
                  
            }
            
            //looking for orphan cells
            for(l=0;l<solids->nsolid;l++){ //loop over cells inside the solid bounding box
                  for(i=solids->stl[l].imin[0];i<=solids->stl[l].imax[0];i++){
                              for(j=solids->stl[l].imin[1];j<=solids->stl[l].imax[1];j++){
                                    for(k=solids->stl[l].imin[2];k<=solids->stl[l].imax[2];k++){
                                    
                                          n= mesh->xcells*j + i + k*mesh->xcells*mesh->ycells;
                                          if(cell[n].ghost!=1){

                                          ct=0;
                                          if(cell[n].type==0){
                                                if(cell[n].w1->cellL->type==0){
                                                      ct+=1;
                                                }
                                                if(cell[n].w2->cellR->type==0){
                                                      ct+=1;
                                                }
                                                if(cell[n].w3->cellR->type==0){
                                                      ct+=1;
                                                }
                                                if(cell[n].w4->cellL->type==0){
                                                      ct+=1;
                                                }
                                                if(cell[n].w5->cellL->type==0){
                                                      ct+=1;
                                                }
                                                if(cell[n].w6->cellR->type==0){
                                                      ct+=1;
                                                }
                                                //if(ct<2||cell[n].out==1){ //when a solid cell has only ONE solid neighbor OR it is ouside the surface, it is re-converted to fluid cell.
                                                if(ct<2){ //when a solid cell has only ONE solid neighbor, it is re-converted to fluid cell.
                                                      cell[n].type=1;
                                                }
                                          
                                          }
                                          
                                          }
                                          
                                          
                                    }
                        }
                  }
            }
            
            //printf("%s Solid cells have been identified \n\n",OK);
            
            
      // distance from all cells to the closest solid cell, in the cartesian directions, is computed
      for(k=0;k<mesh->ncells;k++){
            if(cell[k].type!=0){
            m=cell[k].m;
            n=cell[k].n;
            for (l=0;l<mesh->xcells;l++){
                  na = l + m*mesh->xcells + n*mesh->xcells*mesh->ycells;
                  if(cell[na].type==0){
                        cell[k].distsolx=MIN(cell[k].distsolx,abs(cell[k].l-l))  ; 
                  }
            }
            l=cell[k].l;
            n=cell[k].n;
            for (m=0;m<mesh->ycells;m++){
                  na = l + m*mesh->xcells + n*mesh->xcells*mesh->ycells;
                  if(cell[na].type==0){
                        cell[k].distsoly=MIN(cell[k].distsoly,abs(cell[k].m-m)) ;  
                  }
            }
            l=cell[k].l;
            m=cell[k].m;
            for (n=0;n<mesh->zcells;n++){
                  na = l + m*mesh->xcells + n*mesh->xcells*mesh->ycells;
                  if(cell[na].type==0){
                        cell[k].distsolz=MIN(cell[k].distsolz,abs(cell[k].n-n)) ;  
                  }
            } 
            //if(cell[k].distsoly<100){
            //      printf("celda %d, dist: %d \n",k,cell[k].distsoly); 
            //}
            
            }
      }
      
      
      }
	  

	
#endif
	return 1;
}





int assign_image_cells(t_mesh *mesh,t_solid *solids){
	t_cell *cell;
	int m,l,n,k,i,j,q;
	int imin,imax,jmin,jmax,kmin,kmax;
      double r;
      double dif[3],xc[3],v1[3],v2[3],v3[3],vp1[3],vp2[3],vp3[3],dc1[3],dc2[3],dc3[3];
      double di[8],li[8];
      double aux1,aux2,aux3,sum;

	if(solids->nsolid<1){
            printf("%s In function assign_image_cells() no solids are considered\n",WAR);
      }else{
		
		cell=mesh->cell;
		for(n=0;n<mesh->ncells;n++){
			  if (cell[n].ghost==1) {
				
				if (cell[n].xim>0.0 && cell[n].xim<mesh->Lx && cell[n].yim>0.0 && cell[n].yim<mesh->Ly && cell[n].zim>0.0 && cell[n].zim<mesh->Lz) {  // We first check that image points are inside the computational domain. Habria que usar +- dx para afinar
					
					// The indices of the 8 closest cells to an image point are defined
					imin= MAX((cell[n].xim-mesh->dx/2.0)/mesh->dx,0); 
					imax= MIN(imin+1,mesh->xcells-1);
					jmin= MAX((cell[n].yim-mesh->dy/2.0)/mesh->dy,0);
					jmax= MIN(jmin+1,mesh->ycells-1);
					kmin= MAX((cell[n].zim-mesh->dz/2.0)/mesh->dz,0);
					kmax= MIN(kmin+1,mesh->zcells-1);
					
					
					
					// The cell numbers of those 8 neighbors are defined
					cell[n].ni[0]= imin + jmin*mesh->xcells  + kmin*mesh->xcells*mesh->ycells; 
					cell[n].ni[1]= imax + jmin*mesh->xcells  + kmin*mesh->xcells*mesh->ycells;
					cell[n].ni[2]= imax + jmax*mesh->xcells  + kmin*mesh->xcells*mesh->ycells;
					cell[n].ni[3]= imin + jmax*mesh->xcells  + kmin*mesh->xcells*mesh->ycells;
					cell[n].ni[4]= imin + jmin*mesh->xcells  + kmax*mesh->xcells*mesh->ycells;
					cell[n].ni[5]= imax + jmin*mesh->xcells  + kmax*mesh->xcells*mesh->ycells;
					cell[n].ni[6]= imax + jmax*mesh->xcells  + kmax*mesh->xcells*mesh->ycells;
					cell[n].ni[7]= imin + jmax*mesh->xcells  + kmax*mesh->xcells*mesh->ycells;
					

					// The weights for the interpolation at the image points are computed 
					sum=0.0;
					for(q=0;q<8;q++){ // loop over neighbor cells
						aux1=cell[n].xim-cell[cell[n].ni[q]].xc;
						aux2=cell[n].yim-cell[cell[n].ni[q]].yc;
						aux3=cell[n].zim-cell[cell[n].ni[q]].zc;
						
						//if(n==594473){
						//      printf("cell image : %lf %lf %lf\n", cell[n].xim,cell[n].yim,cell[n].zim );
						//      printf("aux1 %lf aux2 %lf aux3 %lf\n", aux1,aux2,aux3);
						//      getchar();
						//}
						
						di[q]=pow( aux1*aux1+aux2*aux2+aux3*aux3, 0.5 ); // this is the distance between the image point and each neighbor cell
						if(cell[cell[n].ni[q]].ghost!=1){
							li[q]=1.0/(di[q]*di[q]+TOL14);
						}else{
							li[q]=0.0;
						}
						sum = sum+li[q];

					}
					
					if(sum<TOL14){
						cell[n].type=0;
						cell[n].ghost=0;
					}else{
						for(q=0;q<8;q++){
							cell[n].li[q]=li[q]/sum; // weighting coefficients
						}
					}
					
					
	     
				
				}else{
				//if(cell[n].zc<mesh->dz){
				//      printf("cell %d,: %d %d %d \n", n,cell[n].l,cell[n].m,cell[n].n );
				//      printf("cell image : %lf %lf %lf\n", cell[n].xim,cell[n].yim,cell[n].zim );
				//	}
					cell[n].type=0;
					cell[n].ghost=0;
				}
				
			  }		
		}
		
		//printf("%s Image points have been identified \n\n",OK);
	}

	return 1;
}


int update_ghost_cells(t_sim *sim,t_mesh *mesh,t_solid *solids){
	t_cell *cell;
      t_triangle *triangle;
	int m,l,n,k,i,j,q;
      double auxval[sim->nvar];
      double dotprod;    
      
	if(solids->nsolid>0){		
		cell=mesh->cell;
		for(n=0;n<mesh->ncells;n++){
			  if (cell[n].ghost==1) {
				triangle=cell[n].tri;  // triangular facet associated to a ghost cell
				for(k=0;k<sim->nvar;k++){
					auxval[k]=0.0;
					for(q=0;q<8;q++){
						auxval[k]= auxval[k] + cell[n].li[q]* cell[cell[n].ni[q]].U[k]; //interpolated variables at image point
						//printf("li:%lf U:%lf \n", cell[n].li[q],cell[cell[n].ni[q]].U[k] );
					}
					//getchar();
				}
				
				
				dotprod=triangle->nr[0]*auxval[1]+triangle->nr[1]*auxval[2]+triangle->nr[2]*auxval[3];                     
				for(k=0;k<sim->nvar;k++){
					if(k==1||k==2||k==3){
						cell[n].U[k]=auxval[k]-2.0*dotprod*triangle->nr[k-1]; //this is a reflection for vector variables u_r=u-2*(u·n)n, which allows to impose the Dirichlet BC of zero velocity at solid faces
					}else{
						cell[n].U[k]=auxval[k]; //non-vector variables are assigned equal
					}
						
				}

				
				  
			  }
		}
		
		//printf("%s Ghost cell values have been computed \n\n",OK);
      
      }
      
}



int vector_product(double *input1, double *input2, double *output){

      output[0]=input1[1]*input2[2]-input1[2]*input2[1];
      output[1]=input1[2]*input2[0]-input1[0]*input2[2];
      output[2]=input1[0]*input2[1]-input1[1]*input2[0];

	return 1;
}

double dot_product(double *input1, double *input2){

      return input1[0]*input2[0]+input1[1]*input2[1]+input1[2]*input2[2];

}





int assign_wall_type(t_mesh *mesh){ 
	t_wall *wall;
	int m,l,n,k,wp;
      
      
      for(n=0;n<mesh->nwalls;n++){
            wall=&(mesh->wall[n]);
            wall->wtype=1;          //by default: 1= normal RP wall
            wall->boundId=999;    //999 when the wall is not at any boundary. Otherwise: 1, 2, 3, 4, 5, 6.
            
            #if ALLOW_SOLIDS==1   
            
            
            if(wall->nx>TOL4){
                  if(wall->cellL->type==0 && wall->cellR->type!=0){                 
                        wall->wtype=4;
                        wall->boundId=4;
                  }else if(wall->cellR->type==0 && wall->cellL->type!=0){
                        wall->wtype=4;  
                        wall->boundId=2;                       
                  }else if(wall->cellL->type==0 && wall->cellR->type==0){
                        wall->wtype=0; 
                  }        
            }else if(wall->ny>TOL4){
                  if(wall->cellL->type==0 && wall->cellR->type!=0){                 
                        wall->wtype=4;
                        wall->boundId=1;
                  }else if(wall->cellR->type==0 && wall->cellL->type!=0){
                        wall->wtype=4;  
                        wall->boundId=3;                       
                  }else if(wall->cellL->type==0 && wall->cellR->type==0){
                        wall->wtype=0; 
                  }                 
            }else{
                  if(wall->cellL->type==0 && wall->cellR->type!=0){                 
                        wall->wtype=4;
                        wall->boundId=5;
                  }else if(wall->cellR->type==0 && wall->cellL->type!=0){
                        wall->wtype=4;  
                        wall->boundId=6;                      
                  }else if(wall->cellL->type==0 && wall->cellR->type==0){
                        wall->wtype=0; 
                  }     
            }

            
            
            #endif
 
      }
	
	
      
      m=0;
      for(l=0;l<mesh->xcells;l++){
            for(n=0;n<mesh->zcells;n++){
                  //k=mesh->xcells*m+l;
                  k= mesh->xcells*m + l + n*mesh->xcells*mesh->ycells;
                  wall=&(mesh->wall[mesh->cell[k].w1_id]);
                  if(mesh->cell[k].type!=0){
                        wall->wtype=mesh->bc[0]; 
                        wall->boundId=1;
                  }else{
                        wall->wtype=0;
                  }
                  
            }
      }		
            	
      m=mesh->ycells-1;
      for(l=0;l<mesh->xcells;l++){
            for(n=0;n<mesh->zcells;n++){
                  k= mesh->xcells*m + l + n*mesh->xcells*mesh->ycells;
                  wall=&(mesh->wall[mesh->cell[k].w3_id]);
                  if(mesh->cell[k].type!=0){ 
                        wall->wtype=mesh->bc[2]; 
                        wall->boundId=3;
                  }else{
                        wall->wtype=0;
                  }
            }
      }		
      
	
      l=mesh->xcells-1;
      for(m=0;m<mesh->ycells;m++){
            for(n=0;n<mesh->zcells;n++){
                  k= mesh->xcells*m + l + n*mesh->xcells*mesh->ycells;
                  wall=&(mesh->wall[mesh->cell[k].w2_id]);
                  if(mesh->cell[k].type!=0){ 
                        wall->wtype=mesh->bc[1]; 
                        wall->boundId=2;
                  }else{
                        wall->wtype=0;
                  }
            }
      }		
     
	
      l=0; 
      for(m=0;m<mesh->ycells;m++){
            for(n=0;n<mesh->zcells;n++){
                  k= mesh->xcells*m + l + n*mesh->xcells*mesh->ycells;
                  wall=&(mesh->wall[mesh->cell[k].w4_id]);
                  if(mesh->cell[k].type!=0){ 
                        wall->wtype=mesh->bc[3]; 
                        wall->boundId=4;
                  }else{
                        wall->wtype=0;
                  }
            }
      }		

      n=0;
      for(m=0;m<mesh->ycells;m++){
            for(l=0;l<mesh->xcells;l++){
                  k= mesh->xcells*m + l + n*mesh->xcells*mesh->ycells;
                  wall=&(mesh->wall[mesh->cell[k].w5_id]);
                  if(mesh->cell[k].type!=0){ 
                        wall->wtype=mesh->bc[4]; 
                        wall->boundId=5;
                  }else{
                        wall->wtype=0;
                  }
            }
      }		
            	
      n=mesh->zcells-1;;
      for(m=0;m<mesh->ycells;m++){
            for(l=0;l<mesh->xcells;l++){
                  k= mesh->xcells*m + l + n*mesh->xcells*mesh->ycells;
                  wall=&(mesh->wall[mesh->cell[k].w6_id]);
                  if(mesh->cell[k].type!=0){ 
                        wall->wtype=mesh->bc[5]; 
                        wall->boundId=6;
                  }else{
                        wall->wtype=0;
                  }
            }
      }		

      
	return 1;
}





int update_wall_type(t_mesh *mesh,t_solid *solids){ 
	t_wall *wall;
	int m,l,n,k,wp;
      
	if(solids->nsolid<1){
            printf("%s In function update_wall_type() no solids are considered\n",WAR);
      }else{      
		for(n=0;n<mesh->nwalls;n++){
			wall=&(mesh->wall[n]);                  

			if(wall->cellL->ghost>0 && wall->cellR->ghost>0){       //left and right ghost          
				wall->wtype=0;
			}else if(wall->cellR->type==0 && wall->cellL->ghost>0){ //left ghost and right solid
				wall->wtype=0;                        
			}else if(wall->cellL->type==0 && wall->cellR->ghost>0){ //left solid and right ghost
				wall->wtype=0; 
			}else if(wall->cellL->type==0 && wall->cellR->type==0){ //left and right solids
				wall->wtype=0; 
			}          
			 

		}
	
	}

      return 1;
      
}






////////////////////////////////////////////////////
//// C A L C U L A T I O N  F U N C T I O N S //////
////////////////////////////////////////////////////


int compute_fluxes(t_mesh *mesh, t_sim *sim){
	
	double phi3[3],phi5[5],phi7[7]; //auxiliar arrays for the weno reconstruction
	double lambdaMax;
	int order;
	int n,i,j,k;
	int st[9]; //local stencil array
	t_wall *wall;

	mesh->lambda_max=0.0;
	lambdaMax=mesh->lambda_max;
//	wall=mesh->wall;
#pragma omp parallel for default(none) private(wall,phi3,phi5,phi7,order,i,j,k,st) shared(sim,mesh) reduction(max:lambdaMax)
	for(n=0;n<mesh->nwalls;n++){
		wall=&(mesh->wall[n]);
		
            if(wall->wtype!=0){
                
            //RIGHT RECONSTRUCTION
            if(wall->nx<TOL4 && wall->nz<TOL4){
                  //y-wall
                  order=wall->cellR->st_sizeY;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellR->stY[j];
                  }
            }else if (wall->nz<TOL4) {
                  //x-wall
                  order=wall->cellR->st_sizeX;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellR->stX[j];
                  }
            }else{
                  //z-wall
                  order=wall->cellR->st_sizeZ;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellR->stZ[j];
                  }
            }
            if(order==1){

                  for(k=0;k<sim->nvar;k++){
                        wall->UR[k]=wall->cellR->U[k];
                  }

            }else if(order==3){

                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi3[i]=mesh->cell[st[i]].U[k];
                        }
                        wall->UR[k]=weno3R(phi3);
                  }

            }else if(order==5){
                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi5[i]=mesh->cell[st[i]].U[k];
                        }
                        wall->UR[k]=weno5R(phi5);
                  }
            }else if(order==7){
                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi7[i]=mesh->cell[st[i]].U[k];
                        }
                        wall->UR[k]=weno7R(phi7);
                  }      
            }else{
                  //order==9
            }



            //LEFT RECONSTRUCTION
            if(wall->nx<TOL4 && wall->nz<TOL4){
                  //y-wall
                  order=wall->cellL->st_sizeY;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellL->stY[j];
                  }
            }else if (wall->nz<TOL4) {
                  //x-wall
                  order=wall->cellL->st_sizeX;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellL->stX[j];
                  }
            }else{
                  //z-wall
                  order=wall->cellL->st_sizeZ;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellL->stZ[j];
                  }
            }

            if(order==1){

                  for(k=0;k<sim->nvar;k++){
                        wall->UL[k]=wall->cellL->U[k];
                  }

            }else if(order==3){

                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi3[i]=mesh->cell[st[i]].U[k];
                        }
                        wall->UL[k]=weno3L(phi3);
                  }

            }else if(order==5){
                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi5[i]=mesh->cell[st[i]].U[k];
                        }
                        wall->UL[k]=weno5L(phi5);
                  }         
            }else if(order==7){
                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi7[i]=mesh->cell[st[i]].U[k];
                        }
                        wall->UL[k]=weno7L(phi7);
                  }
            }else{
                  //order==7
            }
            
                   
            if(wall->wtype==1){  
            
            //This is to compute fn at each edge
            #if LINEAR_TRANSPORT==0
                  #if EULER
                        #if HLLE
                              compute_euler_HLLE(wall,&lambdaMax);
                        #endif
                        #if HLLC
                              compute_euler_HLLC(wall,&lambdaMax);
                        #endif
				#if HLLS
                              compute_euler_HLLS(wall,&lambdaMax,sim);
                        #endif
                        #if ROE
                              compute_euler_Roe(wall,&lambdaMax);
                        #endif
                  #endif
                  
                  #if SW
                        #if HLLE
                              compute_sw_HLLE(wall,&lambdaMax);
                        #endif
                        #if ROE
                              compute_sw_Roe(wall,&lambdaMax);
                        #endif
                              

                  #endif
            #endif
            
            #if BURGERS
                  compute_burgers_flux(wall,&lambdaMax);
            #endif
            
            #if LINEAR
                  compute_linear_flux(wall,&lambdaMax);
            #endif
      
            
            }else if(wall->wtype==3){
                  if(wall->boundId==999){
                        printf("%s boundId has been assigned 999, please check. The program will close when pressing a key. \n",ERR);
                        getchar();
                        exit(1);
                  }
                  #if EULER
                  compute_transmissive_euler(wall,wall->boundId);  
                  #endif                  
            }else if(wall->wtype==4){
                  if(wall->boundId==999){
                        printf("%s boundId has been assigned 999, please check. The program will close when pressing a key. \n",ERR);
                        getchar();
                        exit(1);
                  }
                  #if EULER
                  compute_solid_euler_hlle(wall,&lambdaMax,wall->boundId); 
                  #endif                   
            }
            #if EULER
		compute_transport(wall); 
            #endif   
            
            
            }
      
	}
      
	mesh->lambda_max=lambdaMax;


	return 1;
}




int equilibrium_reconstruction(t_mesh *mesh, t_sim *sim){
	
	double phi3[3],phi5[5],phi7[7]; //auxiliar arrays for the weno reconstruction
	double uL,uR,vL,vR,wL,wR;
	int order;
	int n,i,j,k;
	int st[9]; //local stencil array
	t_wall *wall;
      t_cell *cell;


#pragma omp parallel for default(none) private(wall,phi3,phi5,phi7,uL,uR,vL,vR,wL,wR,order,i,j,k,st) shared(sim,mesh) 
	for(n=0;n<mesh->nwalls;n++){
		wall=&(mesh->wall[n]);
		
            if(wall->wtype!=0){
                
            //RIGHT RECONSTRUCTION
            if(wall->nx<TOL4 && wall->nz<TOL4){
                  //y-wall
                  order=wall->cellR->st_sizeY;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellR->stY[j];
                  }
            }else if (wall->nz<TOL4) {
                  //x-wall
                  order=wall->cellR->st_sizeX;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellR->stX[j];
                  }
            }else{
                  //z-wall
                  order=wall->cellR->st_sizeZ;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellR->stZ[j];
                  }
            }
            if(order==1){

                  for(k=0;k<sim->nvar;k++){
                        wall->URe[k]=wall->cellR->Ue[k];
                  }

            }else if(order==3){

                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi3[i]=mesh->cell[st[i]].Ue[k];
                        }
                        wall->URe[k]=weno3R(phi3);
                  }

            }else if(order==5){
                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi5[i]=mesh->cell[st[i]].Ue[k];
                        }
                        wall->URe[k]=weno5R(phi5);
                  }
            }else if(order==7){
                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi7[i]=mesh->cell[st[i]].Ue[k];
                        }
                        wall->URe[k]=weno7R(phi7);
                  }      
            }else{
                  //order==9
            }



            //LEFT RECONSTRUCTION
            if(wall->nx<TOL4 && wall->nz<TOL4){
                  //y-wall
                  order=wall->cellL->st_sizeY;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellL->stY[j];
                  }
            }else if (wall->nz<TOL4) {
                  //x-wall
                  order=wall->cellL->st_sizeX;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellL->stX[j];
                  }
            }else{
                  //z-wall
                  order=wall->cellL->st_sizeZ;
                  for(j=0;j<9;j++){
                        st[j]=wall->cellL->stZ[j];
                  }
            }

            if(order==1){

                  for(k=0;k<sim->nvar;k++){
                        wall->ULe[k]=wall->cellL->Ue[k];
                  }

            }else if(order==3){

                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi3[i]=mesh->cell[st[i]].Ue[k];
                        }
                        wall->ULe[k]=weno3L(phi3);
                  }

            }else if(order==5){
                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi5[i]=mesh->cell[st[i]].Ue[k];
                        }
                        wall->ULe[k]=weno5L(phi5);

                  }         
            }else if(order==7){
                  for(k=0;k<sim->nvar;k++){
                        for(i=0;i<order;i++){
                              phi7[i]=mesh->cell[st[i]].Ue[k];
                        }
                        wall->ULe[k]=weno7L(phi7);
                  }
            }else{
                  //order==7
            }
		
		uL=wall->ULe[1]/wall->ULe[0];
		uR=wall->URe[1]/wall->URe[0];

		vL=wall->ULe[2]/wall->ULe[0];
		vR=wall->URe[2]/wall->URe[0];
		
		wL=wall->ULe[3]/wall->ULe[0];
		wR=wall->URe[3]/wall->URe[0];

		wall->pLe=pressure_from_energy(_gamma_, wall->ULe[4], uL, vL, wL, wall->ULe[0], wall->z);
		wall->pRe=pressure_from_energy(_gamma_, wall->URe[4], uR, vR, wR, wall->URe[0], wall->z);
      
	}
      
      }
      
#pragma omp parallel for default(none) private(k,cell) shared(mesh)
	for(i=0;i<mesh->ncells;i++){
		cell=&(mesh->cell[i]);
            if(cell->type!=0){
			cell->S_corr[3] = (cell->w6->pLe-cell->w5->pRe)/cell->dz + _g_*cell->Ue[0];
			//cell->S_corr[3] = (cell->w6->ULe[4]-cell->w5->URe[4])*(_gamma_-1.0)/cell->dz + _g_*cell->Ue[0]; this is only valid for static equilibrium
            }
	}

     
      
	return 1;
}








double weno3R(double *phi){

	double b0, b1;		//beta
	double a0, a1, a_sum;	//alpha
	double g0, g1;		//gamma optimal weight/
	double c0, c1;
	double w0, w1;		//WENO weight
	double UR;
		
	g0=2.0/3.0;
	g1=1.0/3.0;
    
#if TYPE_REC == 0  

	b0=(phi[1]-phi[0])*(phi[1]-phi[0]);
	b1=(phi[2]-phi[1])*(phi[2]-phi[1]);

	a0=g0/((b0+epsilon)*(b0+epsilon));
	a1=g1/((b1+epsilon)*(b1+epsilon));
	
	a_sum=a0+a1;

	w0=a0/a_sum;
	w1=a1/a_sum;
    
#elif TYPE_REC == 1  //TENO

	b0=(phi[1]-phi[0])*(phi[1]-phi[0]);
	b1=(phi[2]-phi[1])*(phi[2]-phi[1]);
    
	a0=1.0/pow((b0+epsilon2),_Q_);
	a1=1.0/pow((b1+epsilon2),_Q_);
	
	c0 = a0/(a0 + a1);
      c1 = a1/(a0 + a1);
      
      c0 = c0 < _CT_ ? 0. : 1.;
      c1 = c1 < _CT_ ? 0. : 1.;
	
	a0 = g0*c0;
      a1 = g1*c1;
	
      w0 = a0/(a0 + a1);
      w1 = a1/(a0 + a1);
    
#else  
    
    w0=g0;
	w1=g1;
    
#endif  
	
	UR=w0*(0.5*phi[1]+0.5*phi[0])+w1*(-0.5*phi[2]+1.5*phi[1]);


	return UR;
}


double weno3L(double *phi){

	double b0, b1;		//beta
	double a0, a1, a_sum;	//alpha
	double g0, g1;		//gamma optimal weight/
	double c0, c1;
	double w0, w1;		//WENO weight
	double UL;
		
	g0=1.0/3.0;
	g1=2.0/3.0;
    
#if TYPE_REC == 0   

	b0=(phi[1]-phi[0])*(phi[1]-phi[0]);
	b1=(phi[2]-phi[1])*(phi[2]-phi[1]);

	a0=g0/((b0+epsilon)*(b0+epsilon));
	a1=g1/((b1+epsilon)*(b1+epsilon));
	
	a_sum=a0+a1;

	w0=a0/a_sum;
	w1=a1/a_sum;

#elif TYPE_REC == 1  //TENO

	b0=(phi[1]-phi[0])*(phi[1]-phi[0]);
	b1=(phi[2]-phi[1])*(phi[2]-phi[1]);
    
	a0=1.0/pow((b0+epsilon2),_Q_);
	a1=1.0/pow((b1+epsilon2),_Q_);
	
	c0 = a0/(a0 + a1);
      c1 = a1/(a0 + a1);
      
      c0 = c0 < _CT_ ? 0. : 1.;
      c1 = c1 < _CT_ ? 0. : 1.;
	
	a0 = g0*c0;
      a1 = g1*c1;
	
      w0 = a0/(a0 + a1);
      w1 = a1/(a0 + a1);
    
#else //UWC 
    
    w0=g0;
	w1=g1;
    
#endif  
	
	UL=w0*(-0.5*phi[0]+1.5*phi[1])+w1*(0.5*phi[1]+0.5*phi[2]);


	return UL;
}


double weno5R(double *phi){

	double b0, b1, b2;		//beta
	double a0, a1, a2, a_sum;	//alpha
	double g0, g1, g2;		//gamma optimal weight/
	double c0, c1, c2;
	double w0, w1, w2;		//WENO weight
	double s55;
	double UR;
		
	g0=3.0/10.0;
	g1=3.0/5.0;
      g2=1.0/10.0;
      
#if TYPE_REC == 0    //WENO

	b0=13.0/12.0*(phi[0]-2*phi[1]+phi[2])*(phi[0]-2*phi[1]+phi[2])+0.25*(phi[0]-4*phi[1]+3*phi[2])*(phi[0]-4*phi[1]+3*phi[2]);
	b1=13.0/12.0*(phi[1]-2*phi[2]+phi[3])*(phi[1]-2*phi[2]+phi[3])+0.25*(phi[1]-phi[3])*(phi[1]-phi[3]);
      b2=13.0/12.0*(phi[2]-2*phi[3]+phi[4])*(phi[2]-2*phi[3]+phi[4])+0.25*(3*phi[2]-4*phi[3]+phi[4])*(3*phi[2]-4*phi[3]+phi[4]);
	
      
      a0=g0/((b0+epsilon)*(b0+epsilon));
	a1=g1/((b1+epsilon)*(b1+epsilon));
      a2=g2/((b2+epsilon)*(b2+epsilon));
	
	a_sum=a0+a1+a2;

	w0=a0/a_sum;
	w1=a1/a_sum;
      w2=a2/a_sum;
      
#elif TYPE_REC == 1  //TENO

	b0=13.0/12.0*(phi[0]-2*phi[1]+phi[2])*(phi[0]-2*phi[1]+phi[2])+0.25*(phi[0]-4*phi[1]+3*phi[2])*(phi[0]-4*phi[1]+3*phi[2]);
	b1=13.0/12.0*(phi[1]-2*phi[2]+phi[3])*(phi[1]-2*phi[2]+phi[3])+0.25*(phi[1]-phi[3])*(phi[1]-phi[3]);
      b2=13.0/12.0*(phi[2]-2*phi[3]+phi[4])*(phi[2]-2*phi[3]+phi[4])+0.25*(3*phi[2]-4*phi[3]+phi[4])*(3*phi[2]-4*phi[3]+phi[4]);

	/*
	s55 = fabs(b2 - b0);
	a0 = pow(1.0 + s55/(b0+epsilon2),_Q_);
      a1 = pow(1.0 + s55/(b1+epsilon2),_Q_);
      a2 = pow(1.0 + s55/(b2+epsilon2),_Q_);
	*/
    
	a0=1.0/pow((b0+epsilon2),_Q_);
	a1=1.0/pow((b1+epsilon2),_Q_);
      a2=1.0/pow((b2+epsilon2),_Q_);
	
	c0 = a0/(a0 + a1 + a2);
      c1 = a1/(a0 + a1 + a2);
      c2 = a2/(a0 + a1 + a2);
      
      c0 = c0 < _CT_ ? 0. : 1.;
      c1 = c1 < _CT_ ? 0. : 1.;
      c2 = c2 < _CT_ ? 0. : 1.;
	
	a0 = g0*c0;
      a1 = g1*c1;
      a2 = g2*c2;
	
      w0 = a0/(a0 + a1 + a2);
      w1 = a1/(a0 + a1 + a2);
      w2 = a2/(a0 + a1 + a2);
    
#else //UWC
	
	w0=g0;
	w1=g1;
	w2=g2;


#endif        
	
	UR=w0*(1.0/3.0*phi[2]+5.0/6.0*phi[1]-1.0/6.0*phi[0]) + w1*(-1.0/6.0*phi[3]+5.0/6.0*phi[2]+1.0/3.0*phi[1]) + w2*(1.0/3.0*phi[4]-7.0/6.0*phi[3]+11.0/6.0*phi[2]) ;


	return UR;
}

double weno5L(double *phi){
	
	double b0, b1, b2;		//beta
	double a0, a1, a2, a_sum;	//alpha
	double g0, g1, g2;		//gamma optimal weight/
	double c0, c1, c2;
	double w0, w1, w2;		//WENO weight
	double s55;
	double UL;
		
	g0=1.0/10.0;
	g1=3.0/5.0;
      g2=3.0/10.0;
    
#if TYPE_REC == 0    

	b0=13.0/12.0*(phi[0]-2*phi[1]+phi[2])*(phi[0]-2*phi[1]+phi[2])+0.25*(phi[0]-4*phi[1]+3*phi[2])*(phi[0]-4*phi[1]+3*phi[2]);
	b1=13.0/12.0*(phi[1]-2*phi[2]+phi[3])*(phi[1]-2*phi[2]+phi[3])+0.25*(phi[1]-phi[3])*(phi[1]-phi[3]);
      b2=13.0/12.0*(phi[2]-2*phi[3]+phi[4])*(phi[2]-2*phi[3]+phi[4])+0.25*(3*phi[2]-4*phi[3]+phi[4])*(3*phi[2]-4*phi[3]+phi[4]);

     
      a0=g0/((b0+epsilon)*(b0+epsilon));
	a1=g1/((b1+epsilon)*(b1+epsilon));
      a2=g2/((b2+epsilon)*(b2+epsilon));
	
	a_sum=a0+a1+a2;

	w0=a0/a_sum;
	w1=a1/a_sum;
      w2=a2/a_sum;
	
#elif TYPE_REC == 1  //TENO
 
	b0=13.0/12.0*(phi[0]-2*phi[1]+phi[2])*(phi[0]-2*phi[1]+phi[2])+0.25*(phi[0]-4*phi[1]+3*phi[2])*(phi[0]-4*phi[1]+3*phi[2]);
	b1=13.0/12.0*(phi[1]-2*phi[2]+phi[3])*(phi[1]-2*phi[2]+phi[3])+0.25*(phi[1]-phi[3])*(phi[1]-phi[3]);
      b2=13.0/12.0*(phi[2]-2*phi[3]+phi[4])*(phi[2]-2*phi[3]+phi[4])+0.25*(3*phi[2]-4*phi[3]+phi[4])*(3*phi[2]-4*phi[3]+phi[4]);
   
	/*
	s55 = fabs(b2 - b0);
	a0 = pow(1.0 + s55/(b0+epsilon2),_Q_);
      a1 = pow(1.0 + s55/(b1+epsilon2),_Q_);
      a2 = pow(1.0 + s55/(b2+epsilon2),_Q_);
	*/
    
	a0=1.0/pow((b0+epsilon2),_Q_);
	a1=1.0/pow((b1+epsilon2),_Q_);
      a2=1.0/pow((b2+epsilon2),_Q_);
	
	c0 = a0/(a0 + a1 + a2);
      c1 = a1/(a0 + a1 + a2);
      c2 = a2/(a0 + a1 + a2);
      
      c0 = c0 < _CT_ ? 0. : 1.;
      c1 = c1 < _CT_ ? 0. : 1.;
      c2 = c2 < _CT_ ? 0. : 1.;
	
	a0 = g0*c0;
      a1 = g1*c1;
      a2 = g2*c2;
	
      w0 = a0/(a0 + a1 + a2);
      w1 = a1/(a0 + a1 + a2);
      w2 = a2/(a0 + a1 + a2);
 
#else 
    
      w0=g0;
	w1=g1;
      w2=g2;
    
#endif    
      
	
	UL=w2*(1.0/3.0*phi[2]+5.0/6.0*phi[3]-1.0/6.0*phi[4]) + w1*(-1.0/6.0*phi[1]+5.0/6.0*phi[2]+1.0/3.0*phi[3]) + w0*(1.0/3.0*phi[0]-7.0/6.0*phi[1]+11.0/6.0*phi[2]) ;

    

	return UL;
}




double weno7R(double *phi){

	double b0, b1, b2, b3;		//beta
	double a0, a1, a2, a3, a_sum;	//alpha
	double g0, g1, g2, g3;		//gamma optimal weight/
	double c0, c1, c2, c3;	
	double w0, w1, w2, w3;		//WENO weight
	double UR;
		
	g0=4.0/35.0;
	g1=18.0/35.0;
      g2=12.0/35.0;
      g3=1.0/35.0;

      
#if TYPE_REC == 0    
 
      b0 = phi[0]*(547.0*phi[0] - 3882.0*phi[1] + 4642.0*phi[2] - 1854.0*phi[3]) + phi[1]*(7043.0*phi[1] - 17246.0*phi[2] + 7042.0*phi[3]) + phi[2]*(11003.0*phi[2] - 9402.0*phi[3]) + phi[3]*2107.0*phi[3];		
	b1 = phi[1]*(267.0*phi[1] - 1642.0*phi[2] + 1602.0*phi[3] - 494.0*phi[4]) + phi[2]*(2843.0*phi[2] - 5966.0*phi[3] + 1922.0*phi[4]) + phi[3]*(3443.0*phi[3] - 2522.0*phi[4]) + phi[4]*547.0*phi[4];
	b2 = phi[2]*(547.0*phi[2] - 2522.0*phi[3] + 1922.0*phi[4] - 494.0*phi[5]) + phi[3]*(3443.0*phi[3] - 5966.0*phi[4] + 1602*phi[5]) + phi[4]*(2843.0*phi[4] - 1642*phi[5]) + phi[5]*267.0*phi[5];
	b3 = phi[3]*(2107.0*phi[3] - 9402.0*phi[4] + 7042.0*phi[5] - 1854.0*phi[6]) + phi[4]*(11003.0*phi[4] - 17246.0*phi[5] + 4642.0*phi[6]) + phi[5]*(7043.0*phi[5] - 3882.0*phi[6]) + phi[6]*547.0*phi[6];
   
      a0=g0/((b0+epsilon)*(b0+epsilon));
	a1=g1/((b1+epsilon)*(b1+epsilon));
      a2=g2/((b2+epsilon)*(b2+epsilon));
      a3=g3/((b3+epsilon)*(b3+epsilon));
	
	a_sum=a0+a1+a2+a3;

	w0=a0/a_sum;
	w1=a1/a_sum;
      w2=a2/a_sum;
      w3=a3/a_sum;
      
#elif TYPE_REC == 1  //TENO

      b0 = phi[0]*(547.0*phi[0] - 3882.0*phi[1] + 4642.0*phi[2] - 1854.0*phi[3]) + phi[1]*(7043.0*phi[1] - 17246.0*phi[2] + 7042.0*phi[3]) + phi[2]*(11003.0*phi[2] - 9402.0*phi[3]) + phi[3]*2107.0*phi[3];		
	b1 = phi[1]*(267.0*phi[1] - 1642.0*phi[2] + 1602.0*phi[3] - 494.0*phi[4]) + phi[2]*(2843.0*phi[2] - 5966.0*phi[3] + 1922.0*phi[4]) + phi[3]*(3443.0*phi[3] - 2522.0*phi[4]) + phi[4]*547.0*phi[4];
	b2 = phi[2]*(547.0*phi[2] - 2522.0*phi[3] + 1922.0*phi[4] - 494.0*phi[5]) + phi[3]*(3443.0*phi[3] - 5966.0*phi[4] + 1602*phi[5]) + phi[4]*(2843.0*phi[4] - 1642*phi[5]) + phi[5]*267.0*phi[5];
	b3 = phi[3]*(2107.0*phi[3] - 9402.0*phi[4] + 7042.0*phi[5] - 1854.0*phi[6]) + phi[4]*(11003.0*phi[4] - 17246.0*phi[5] + 4642.0*phi[6]) + phi[5]*(7043.0*phi[5] - 3882.0*phi[6]) + phi[6]*547.0*phi[6];
    
	a0=1.0/pow((b0+epsilon2),_Q_);
	a1=1.0/pow((b1+epsilon2),_Q_);
      a2=1.0/pow((b2+epsilon2),_Q_);
	a3=1.0/pow((b3+epsilon2),_Q_);
	
	c0 = a0/(a0 + a1 + a2 + a3);
      c1 = a1/(a0 + a1 + a2 + a3);
      c2 = a2/(a0 + a1 + a2 + a3);
	c3 = a3/(a0 + a1 + a2 + a3);
      
      c0 = c0 < _CT_ ? 0. : 1.;
      c1 = c1 < _CT_ ? 0. : 1.;
      c2 = c2 < _CT_ ? 0. : 1.;
	c3 = c3 < _CT_ ? 0. : 1.;
	
	a0 = g0*c0;
      a1 = g1*c1;
      a2 = g2*c2;
	a3 = g3*c3;
	
      w0 = a0/(a0 + a1 + a2 + a3);
      w1 = a1/(a0 + a1 + a2 + a3);
      w2 = a2/(a0 + a1 + a2 + a3);
	w3 = a3/(a0 + a1 + a2 + a3);
    
#else //UWC  
    
      w0=g0;
      w1=g1;
      w2=g2;
      w3=g3;
    
#endif        
	
	UR= w0*(1.0/4.0*phi[3]  + 13.0/12.0*phi[2] - 5.0/12.0*phi[1] + 1.0/12.0*phi[0]) + w1*(-1.0/12.0*phi[4] + 7.0/12.0*phi[3] + 7.0/12.0*phi[2] - 1.0/12.0*phi[1]) + w2*(1.0/12.0*phi[5] - 5.0/12.0*phi[4] + 13.0/12.0*phi[3] + 1.0/4.0*phi[2]) + w3*(-1.0/4.0*phi[6] + 13.0/12.0*phi[5] - 23.0/12.0*phi[4] + 25.0/12.0*phi[3]);


	return UR;
}


double weno7L(double *phi){

	double b0, b1, b2, b3;		//beta
	double a0, a1, a2, a3, a_sum;	//alpha
	double g0, g1, g2, g3;		//gamma optimal weight/
	double c0, c1, c2, c3;	
	double w0, w1, w2, w3;		//WENO weight
	double UL;
		
	g0=1.0/35.0;
	g1=12.0/35.0;
      g2=18.0/35.0;
      g3=4.0/35.0;
      
      
#if TYPE_REC == 0    
 
      b0 = phi[0]*(547.0*phi[0] - 3882.0*phi[1] + 4642.0*phi[2] - 1854.0*phi[3]) + phi[1]*(7043.0*phi[1] - 17246.0*phi[2] + 7042.0*phi[3]) + phi[2]*(11003.0*phi[2] - 9402.0*phi[3]) + phi[3]*2107.0*phi[3];		
	b1 = phi[1]*(267.0*phi[1] - 1642.0*phi[2] + 1602.0*phi[3] - 494.0*phi[4]) + phi[2]*(2843.0*phi[2] - 5966.0*phi[3] + 1922.0*phi[4]) + phi[3]*(3443.0*phi[3] - 2522.0*phi[4]) + phi[4]*547.0*phi[4];
	b2 = phi[2]*(547.0*phi[2] - 2522.0*phi[3] + 1922.0*phi[4] - 494.0*phi[5]) + phi[3]*(3443.0*phi[3] - 5966.0*phi[4] + 1602*phi[5]) + phi[4]*(2843.0*phi[4] - 1642*phi[5]) + phi[5]*267.0*phi[5];
	b3 = phi[3]*(2107.0*phi[3] - 9402.0*phi[4] + 7042.0*phi[5] - 1854.0*phi[6]) + phi[4]*(11003.0*phi[4] - 17246.0*phi[5] + 4642.0*phi[6]) + phi[5]*(7043.0*phi[5] - 3882.0*phi[6]) + phi[6]*547.0*phi[6];
   
      a0=g0/((b0+epsilon)*(b0+epsilon));
	a1=g1/((b1+epsilon)*(b1+epsilon));
      a2=g2/((b2+epsilon)*(b2+epsilon));
      a3=g3/((b3+epsilon)*(b3+epsilon));
	
	a_sum=a0+a1+a2+a3;

	w0=a0/a_sum;
	w1=a1/a_sum;
      w2=a2/a_sum;
      w3=a3/a_sum;
      
#elif TYPE_REC == 1  //TENO

      b0 = phi[0]*(547.0*phi[0] - 3882.0*phi[1] + 4642.0*phi[2] - 1854.0*phi[3]) + phi[1]*(7043.0*phi[1] - 17246.0*phi[2] + 7042.0*phi[3]) + phi[2]*(11003.0*phi[2] - 9402.0*phi[3]) + phi[3]*2107.0*phi[3];		
	b1 = phi[1]*(267.0*phi[1] - 1642.0*phi[2] + 1602.0*phi[3] - 494.0*phi[4]) + phi[2]*(2843.0*phi[2] - 5966.0*phi[3] + 1922.0*phi[4]) + phi[3]*(3443.0*phi[3] - 2522.0*phi[4]) + phi[4]*547.0*phi[4];
	b2 = phi[2]*(547.0*phi[2] - 2522.0*phi[3] + 1922.0*phi[4] - 494.0*phi[5]) + phi[3]*(3443.0*phi[3] - 5966.0*phi[4] + 1602*phi[5]) + phi[4]*(2843.0*phi[4] - 1642*phi[5]) + phi[5]*267.0*phi[5];
	b3 = phi[3]*(2107.0*phi[3] - 9402.0*phi[4] + 7042.0*phi[5] - 1854.0*phi[6]) + phi[4]*(11003.0*phi[4] - 17246.0*phi[5] + 4642.0*phi[6]) + phi[5]*(7043.0*phi[5] - 3882.0*phi[6]) + phi[6]*547.0*phi[6];
   
	a0=1.0/pow((b0+epsilon2),_Q_);
	a1=1.0/pow((b1+epsilon2),_Q_);
      a2=1.0/pow((b2+epsilon2),_Q_);
	a3=1.0/pow((b3+epsilon2),_Q_);
	
	c0 = a0/(a0 + a1 + a2 + a3);
      c1 = a1/(a0 + a1 + a2 + a3);
      c2 = a2/(a0 + a1 + a2 + a3);
	c3 = a3/(a0 + a1 + a2 + a3);
      
      c0 = c0 < _CT_ ? 0. : 1.;
      c1 = c1 < _CT_ ? 0. : 1.;
      c2 = c2 < _CT_ ? 0. : 1.;
	c3 = c3 < _CT_ ? 0. : 1.;
	
	a0 = g0*c0;
      a1 = g1*c1;
      a2 = g2*c2;
	a3 = g3*c3;
	
      w0 = a0/(a0 + a1 + a2 + a3);
      w1 = a1/(a0 + a1 + a2 + a3);
      w2 = a2/(a0 + a1 + a2 + a3);
	w3 = a3/(a0 + a1 + a2 + a3);
    
#else //UWC    
    
      w0=g0;
      w1=g1;
      w2=g2;
      w3=g3;
    
#endif        
	
      UL = w0*(-1.0/4.0*phi[0] + 13.0/12.0*phi[1] - 23.0/12.0*phi[2] + 25.0/12.0*phi[3]) + w1*(1.0/12.0*phi[1] - 5.0/12.0*phi[2] + 13.0/12.0*phi[3] + 1.0/4.0*phi[4]) + w2*(-1.0/12.0*phi[2] + 7.0/12.0*phi[3]  + 7.0/12.0*phi[4] - 1.0/12.0*phi[5]) + w3*(1.0/4.0*phi[3] + 13.0/12.0*phi[4] - 5.0/12.0*phi[5] + 1.0/12.0*phi[6]);


	return UL;
}


void set_velocity_field(t_mesh *mesh, t_sim *sim){
	/**This is only for linear transport and
	 * it only works with rho = 1.0 
	 * */
	
	int i;
	t_wall *wall;
	t_cell *cell;
	double lambda_max,dl;
	double rho;
	
	rho=1.0;
	cell=mesh->cell;
	for(i=0;i<mesh->ncells;i++){
		cell->U[0]=rho;
		cell++;
	}
	lambda_max=0.0;
	wall=mesh->wall;
	for(i=0;i<mesh->nwalls;i++){
            if(wall->nx<TOL4 && wall->nz<TOL4){
            //y-wall
                  wall->fR_star[0]=(wall->cellR->U[2]+wall->cellL->U[2])*0.5;
                  wall->fL_star[0]= wall->fR_star[0];
            }else if (wall->nz<TOL4) {
                  //x-wall
                  wall->fR_star[0]=(wall->cellR->U[1]+wall->cellL->U[1])*0.5;
                  wall->fL_star[0]= wall->fR_star[0];
            }else{
                  //z-wall
                  wall->fR_star[0]=(wall->cellR->U[3]+wall->cellL->U[3])*0.5;
                  wall->fL_star[0]= wall->fR_star[0];
            }

		lambda_max=MAX(lambda_max,wall->fR_star[0]);
		wall++;
	}
	
	dl=MIN(mesh->dx,mesh->dy);

	sim->dt=sim->CFL*dl/lambda_max;
      //printf("dt is: %lf",sim->dt);
      //getchar();
}


void set_velocity(t_mesh *mesh, t_sim *sim){
	/**This is only for linear transport and
	 * it only works with rho = 1.0 
	 * */
	
	int i;
	t_wall *wall;
	t_cell *cell;
	
	wall=mesh->wall;
	for(i=0;i<mesh->nwalls;i++){
            if(wall->nx<TOL4 && wall->nz<TOL4){
            //y-wall
                  wall->vel=mesh->u_y;
            }else if (wall->nz<TOL4) {
                  //x-wall
                  wall->vel=mesh->u_x;
            }else{
                  //z-wall
                  wall->vel=mesh->u_z;
            }

		wall++;
	}
	
}



void compute_euler_HLLE(t_wall *wall,double *lambda_max){

	int m;
	double WR[6], WL[6]; /**<Auxiliar array of variables rotated for the 1D problem**/
	double WRe[6], WLe[6];
	double WRprime[6], WLprime[6];
	double uL, uR, vL, vR, wL, wR, pL, pR, HL, HR, cL, cR, gammaL, gammaR, phiL, phiR;
	double pLe, pRe, rhoprimeL, rhoprimeR, EprimeL, EprimeR;
	double raizrhoR, raizrhoL, sumRaizRho;
	double u_hat, v_hat, w_hat, H_hat, c_hat, gamma_hat;
	double S1, S2, diffS, maxS;
	double FR[5], FL[5];
	double F_star[5];

	/**Rotation of array of variables**/
	
	WR[0]=wall->UR[0];
	WL[0]=wall->UL[0];

      // This is a simplification of the rotation matrix, only valid for cartesian mesh
	WR[1]=wall->UR[1]*wall->nx+wall->UR[2]*wall->ny+wall->UR[3]*wall->nz;
	WL[1]=wall->UL[1]*wall->nx+wall->UL[2]*wall->ny+wall->UL[3]*wall->nz;

	WR[2]=-wall->UR[1]*wall->ny+wall->UR[2]*wall->nx+wall->UR[2]*wall->nz;
	WL[2]=-wall->UL[1]*wall->ny+wall->UL[2]*wall->nx+wall->UL[2]*wall->nz;
      
      WR[3]=wall->UR[3]*wall->nx+wall->UR[3]*wall->ny-wall->UR[1]*wall->nz;
	WL[3]=wall->UL[3]*wall->nx+wall->UL[3]*wall->ny-wall->UL[1]*wall->nz;

	WR[4]=wall->UR[4];
	WL[4]=wall->UL[4];
	
	
	#if ST==2||ST==3
	WRe[0]=wall->URe[0];
	WLe[0]=wall->ULe[0];

	WRe[1]=wall->URe[1]*wall->nx+wall->URe[2]*wall->ny+wall->URe[3]*wall->nz;
	WLe[1]=wall->ULe[1]*wall->nx+wall->ULe[2]*wall->ny+wall->ULe[3]*wall->nz;

	WRe[2]=-wall->URe[1]*wall->ny+wall->URe[2]*wall->nx+wall->URe[2]*wall->nz;
	WLe[2]=-wall->ULe[1]*wall->ny+wall->ULe[2]*wall->nx+wall->ULe[2]*wall->nz;
      
      WRe[3]=wall->URe[3]*wall->nx+wall->URe[3]*wall->ny-wall->URe[1]*wall->nz;
	WLe[3]=wall->ULe[3]*wall->nx+wall->ULe[3]*wall->ny-wall->ULe[1]*wall->nz;

	WRe[4]=wall->URe[4];
	WLe[4]=wall->ULe[4];
	#endif
	
	/*
	#if ST==2
		rhoprimeR=WR[0]-wall->URe[0];
		rhoprimeL=WL[0]-wall->ULe[0];
		EprimeR=WR[4]-wall->URe[4];
		EprimeL=WL[4]-wall->ULe[4];	
      #endif */
	for(m=0;m<5;m++){
		WRprime[m]=WR[m];
		WLprime[m]=WL[m];
	}
	#if ST==2||ST==3
		WRprime[0]=WR[0]-WRe[0];
		WLprime[0]=WL[0]-WLe[0];
		WRprime[2]=WR[2]-WRe[2];
		WLprime[2]=WL[2]-WLe[2];
		WRprime[3]=WR[3]-WRe[3];
		WLprime[3]=WL[3]-WLe[3];
		WRprime[4]=WR[4]-WRe[4];
		WLprime[4]=WL[4]-WLe[4];	
      #endif
	
#if MULTICOMPONENT
	WR[5]=wall->UR[5];
	WL[5]=wall->UL[5];
	phiL=WL[5]/WL[0];
	phiR=WR[5]/WR[0];
	#if MULTI_TYPE==1
		gammaL=phiL;
		gammaR=phiR;
	#else
		gammaL=1.0+1.0/phiL;
		gammaR=1.0+1.0/phiR;
	#endif
#else
	gammaL=_gamma_;
	gammaR=_gamma_;
#endif	
	
	/**Additional variables for the solver**/
	uL=WL[1]/WL[0];
	uR=WR[1]/WR[0];

	vL=WL[2]/WL[0];
	vR=WR[2]/WR[0];
      
      wL=WL[3]/WL[0];
	wR=WR[3]/WR[0];

	pL=pressure_from_energy(gammaL, WL[4], uL, vL, wL, WL[0], wall->z);
	pR=pressure_from_energy(gammaR, WR[4], uR, vR, wR, WR[0], wall->z);
	
#if ST==3	
	HL=(WL[4]-WL[0]*_g_*wall->z+pL)/WL[0];
	HR=(WR[4]-WR[0]*_g_*wall->z+pR)/WR[0];
#else
	HL=(WL[4]+pL)/WL[0];
	HR=(WR[4]+pR)/WR[0];
#endif
	
	cL=sqrt(gammaL*pL/WL[0]);

	cR=sqrt(gammaR*pR/WR[0]);
	
	raizrhoL=sqrt(WL[0]);
	raizrhoR=sqrt(WR[0]);
	sumRaizRho=raizrhoR+raizrhoL;

	/**Hat variables (Roe averages)**/
	u_hat=(uR*raizrhoR+uL*raizrhoL)/sumRaizRho;
	v_hat=(vR*raizrhoR+vL*raizrhoL)/sumRaizRho;
      w_hat=(wR*raizrhoR+wL*raizrhoL)/sumRaizRho;
	H_hat=(HR*raizrhoR+HL*raizrhoL)/sumRaizRho;
#if MULTICOMPONENT	
	#if MULTI_TYPE==1
		gamma_hat=1.0+ 1.0/( (phiR*raizrhoR+phiL*raizrhoL)/sumRaizRho );
	#else
		gamma_hat=(gammaR*raizrhoR+gammaL*raizrhoL)/sumRaizRho;
	#endif
#else
	gamma_hat=_gamma_;
#endif

	c_hat=sqrt((gamma_hat-1)*(H_hat-0.5*(u_hat*u_hat+v_hat*v_hat+w_hat*w_hat)));

	/**Physical flux calculation (the F of the eqs.)**/

	FR[0]=WR[1];
	FL[0]=WL[1];
	
#if ST==2||ST==3
      pRe  =wall->pRe; 
      pLe  =wall->pLe; 
	FR[1]=WR[1]*uR+(pR-pRe);
	FL[1]=WL[1]*uL+(pL-pLe);
#else	
	FR[1]=WR[1]*uR+pR;
	FL[1]=WL[1]*uL+pL;
#endif	

	FR[2]=WR[1]*vR;
	FL[2]=WL[1]*vL;
      
      FR[3]=WR[1]*wR;
	FL[3]=WL[1]*wL;
	
	FR[4]=uR*(WR[4]+pR);
	FL[4]=uL*(WL[4]+pL);

/*#if ST==2
	WL[0]=rhoprimeL;
	WR[0]=rhoprimeR;
	WL[4]=EprimeL;
	WR[4]=EprimeR;
#endif*/

	/**Wave speed estimation**/
	
	S1=MIN(uL-cL,u_hat-c_hat);
	S2=MAX(uR+cR, u_hat+c_hat);

	maxS=MAX(ABS(S1),ABS(S2));
	diffS=S2-S1;

	/**HLLE flux calculation**/
	for(m=0;m<5;m++){
		if(S1>=0){
			F_star[m]=FL[m];
		}else if(S2<=0){
			F_star[m]=FR[m];
		}else{
                  F_star[m]=(S2*FL[m]-S1*FR[m]+S1*S2*(WRprime[m]-WLprime[m]))/(diffS);
            }
	}
	
	/**Inverse rotation of the flux**/
	wall->fR_star[0]=F_star[0]; //Mass is not vectorial
	wall->fR_star[1]=F_star[1]*wall->nx - F_star[2]*wall->ny - F_star[3]*wall->nz;
	wall->fR_star[2]=F_star[1]*wall->ny + F_star[2]*wall->nx + F_star[2]*wall->nz;
      wall->fR_star[3]=F_star[3]*wall->nx + F_star[3]*wall->ny + F_star[1]*wall->nz;
	wall->fR_star[4]=F_star[4]; //Energy is not vectorial
	
      for(m=0;m<5;m++){
            wall->fL_star[m]=wall->fR_star[m];
      }
	
	//printf("%14.14e %14.14e %14.14e\n",FR[0],FL[0],F_star[0]);
      
      
	*lambda_max=MAX(*lambda_max,maxS);
		
}




void compute_euler_HLLC(t_wall *wall,double *lambda_max){

	int m;
	double WR[5], WL[5]; /**<Auxiliar array of variables rotated for the 1D problem**/
	double WRe[6], WLe[6];
	double WRprime[6], WLprime[6];
	double uL, uR, vL, vR, wR, wL, pL, pR, HL, HR, cL, cR;
	double pLe, pRe;
	double raizrhoR, raizrhoL, sumRaizRho;
	double u_hat, v_hat, w_hat, H_hat, c_hat;
	double S1, S2, diffS, maxS, S_star;
      double uK, vK, wK, rhoK, SK, pK, EK, aux, rhoK2;
	double FR[5], FL[5];
	double F_star[5],W_star[5];

	/**Rotation of array of variables**/
	
	WR[0]=wall->UR[0];
	WL[0]=wall->UL[0];

      // This is a simplification of the rotation matrix, only valid for cartesian mesh
	WR[1]=wall->UR[1]*wall->nx+wall->UR[2]*wall->ny+wall->UR[3]*wall->nz;
	WL[1]=wall->UL[1]*wall->nx+wall->UL[2]*wall->ny+wall->UL[3]*wall->nz;

	WR[2]=-wall->UR[1]*wall->ny+wall->UR[2]*wall->nx+wall->UR[2]*wall->nz;
	WL[2]=-wall->UL[1]*wall->ny+wall->UL[2]*wall->nx+wall->UL[2]*wall->nz;
      
      WR[3]=wall->UR[3]*wall->nx+wall->UR[3]*wall->ny-wall->UR[1]*wall->nz;
	WL[3]=wall->UL[3]*wall->nx+wall->UL[3]*wall->ny-wall->UL[1]*wall->nz;
      
	WR[4]=wall->UR[4];
	WL[4]=wall->UL[4];
	
	
	#if ST==2||ST==3
	WRe[0]=wall->URe[0];
	WLe[0]=wall->ULe[0];

	WRe[1]=wall->URe[1]*wall->nx+wall->URe[2]*wall->ny+wall->URe[3]*wall->nz;
	WLe[1]=wall->ULe[1]*wall->nx+wall->ULe[2]*wall->ny+wall->ULe[3]*wall->nz;

	WRe[2]=-wall->URe[1]*wall->ny+wall->URe[2]*wall->nx+wall->URe[2]*wall->nz;
	WLe[2]=-wall->ULe[1]*wall->ny+wall->ULe[2]*wall->nx+wall->ULe[2]*wall->nz;
      
      WRe[3]=wall->URe[3]*wall->nx+wall->URe[3]*wall->ny-wall->URe[1]*wall->nz;
	WLe[3]=wall->ULe[3]*wall->nx+wall->ULe[3]*wall->ny-wall->ULe[1]*wall->nz;

	WRe[4]=wall->URe[4];
	WLe[4]=wall->ULe[4];
	#endif
	
	for(m=0;m<5;m++){
		WRprime[m]=WR[m];
		WLprime[m]=WL[m];
	}
	#if ST==2||ST==3
		WRprime[0]=WR[0]-WRe[0];
		WLprime[0]=WL[0]-WLe[0];
		WRprime[2]=WR[2]-WRe[2];
		WLprime[2]=WL[2]-WLe[2];
		WRprime[3]=WR[3]-WRe[3];
		WLprime[3]=WL[3]-WLe[3];
		WRprime[4]=WR[4]-WRe[4];
		WLprime[4]=WL[4]-WLe[4];	
      #endif
	
	/**Additional variables for the solver**/
	uL=WL[1]/WL[0];
	uR=WR[1]/WR[0];

	vL=WL[2]/WL[0];
	vR=WR[2]/WR[0];
      
      wL=WL[3]/WL[0];
	wR=WR[3]/WR[0];

	pL=(_gamma_-1.0)*(WL[4]-0.5*WL[0]*(uL*uL+vL*vL+wL*wL));
	pR=(_gamma_-1.0)*(WR[4]-0.5*WR[0]*(uR*uR+vR*vR+wR*wR));
	
	HL=(WL[4]+pL)/WL[0];
	HR=(WR[4]+pR)/WR[0];
	
	cL=sqrt(_gamma_*pL/WL[0]);

	cR=sqrt(_gamma_*pR/WR[0]);
	
	raizrhoL=sqrt(WL[0]);
	raizrhoR=sqrt(WR[0]);
	sumRaizRho=raizrhoR+raizrhoL;

	/**Hat variables (Roe averages)**/
	u_hat=(uR*raizrhoR+uL*raizrhoL)/sumRaizRho;
	v_hat=(vR*raizrhoR+vL*raizrhoL)/sumRaizRho;
      w_hat=(wR*raizrhoR+wL*raizrhoL)/sumRaizRho;
	H_hat=(HR*raizrhoR+HL*raizrhoL)/sumRaizRho;

	c_hat=sqrt((_gamma_-1)*(H_hat-0.5*(u_hat*u_hat+v_hat*v_hat+w_hat*w_hat)));

	/**Physical flux calculation (the F of the eqs.)**/

	FR[0]=WR[1];
	FL[0]=WL[1];

#if ST==2 //no funciona...
      pRe  =wall->pRe; 
      pLe  =wall->pLe; 
	FR[1]=WR[1]*uR+(pR-pRe);
	FL[1]=WL[1]*uL+(pL-pLe);
#else	
	FR[1]=WR[1]*uR+pR;
	FL[1]=WL[1]*uL+pL;
#endif

	FR[2]=WR[1]*vR;
	FL[2]=WL[1]*vL;
      
      FR[3]=WR[1]*wR;
	FL[3]=WL[1]*wL;

	FR[4]=uR*(WR[4]+pR);
	FL[4]=uL*(WL[4]+pL);


	/**Wave speed estimation**/
	
	S1=MIN(uL-cL,u_hat-c_hat);
	S2=MAX(uR+cR, u_hat+c_hat);
      
      //S1=MIN(0.0,S1);
      //S2=MAX(0.0,S2);


	maxS=MAX(ABS(S1),ABS(S2));
	diffS=S2-S1;
      
      S_star= ( pR-pL + WL[1]*(S1-uL) - WR[1]*(S2-uR) ) / ( WL[0]*(S1-uL) - WR[0]*(S2-uR) );

	/**HLLC flux calculation**/
	
      if(S1>=0){
            for(m=0;m<5;m++){
                  F_star[m]=FL[m];
            }
      }else if(S2<=0){
            for(m=0;m<5;m++){
                  F_star[m]=FR[m];
            }
      }else{
            if(S_star<=0){
                  uK=uR ;
                  vK=vR;
                  wK=wR;
                  rhoK=WRprime[0];
			rhoK2=WR[0];
                  SK=S2;
                  pK=pR;
                  EK=WR[4];
            }else{
                  uK=uL; 
                  vK=vL;
                  wK=wL;
                  rhoK=WLprime[0];
			rhoK2=WL[0];
                  SK=S1;
                  pK=pL;
                  EK=WL[4];            
            }
            aux=rhoK*(SK-uK)/(SK-S_star);
            W_star[0]=aux;
            W_star[1]=aux*S_star;
            W_star[2]=aux*vK;
            W_star[3]=aux*wK;
            W_star[4]=aux*( EK/rhoK2 + (S_star-uK)*(S_star + pK/(rhoK2*(SK-uK))) );
            for(m=0;m<5;m++){
                  if(S_star<=0){
                        F_star[m]=FR[m]+S2*(W_star[m]-WRprime[m]);
                  }else{
                        F_star[m]=FL[m]+S1*(W_star[m]-WLprime[m]);
                  }
            }
            
      }
	
	    
      /**Inverse rotation of the flux**/
	wall->fR_star[0]=F_star[0]; //Mass is not vectorial
	wall->fR_star[1]=F_star[1]*wall->nx - F_star[2]*wall->ny - F_star[3]*wall->nz;
	wall->fR_star[2]=F_star[1]*wall->ny + F_star[2]*wall->nx + F_star[2]*wall->nz;
      wall->fR_star[3]=F_star[3]*wall->nx + F_star[3]*wall->ny + F_star[1]*wall->nz;
	wall->fR_star[4]=F_star[4]; //Energy is not vectorial
	
      for(m=0;m<5;m++){
            wall->fL_star[m]=wall->fR_star[m];
      }
      
      
	*lambda_max=MAX(*lambda_max,maxS);
		
}





void compute_euler_HLLS(t_wall *wall,double *lambda_max, t_sim *sim){

	int m;
	double WR[6], WL[6], S[6], B[6]; /**<Auxiliar array of variables rotated for the 1D problem**/
	double uL, uR, vL, vR, wL, wR, pL, pR, HL, HR, cL, cR, gammaL, gammaR, phiL, phiR;
      double pLe, pRe, rhoLe, rhoRe;
	double raizrhoR, raizrhoL, sumRaizRho, r;
	double u_hat, v_hat, w_hat, H_hat, c_hat, gamma_hat,psi,chi;
	double S1, S2, diffS, maxS;
	double FR[5], FL[5];
	double F_star[5];

	/**Rotation of array of variables**/
	
	WR[0]=wall->UR[0];
	WL[0]=wall->UL[0];

      // This is a simplification of the rotation matrix, only valid for cartesian mesh
	WR[1]=wall->UR[1]*wall->nx+wall->UR[2]*wall->ny+wall->UR[3]*wall->nz;
	WL[1]=wall->UL[1]*wall->nx+wall->UL[2]*wall->ny+wall->UL[3]*wall->nz;

	WR[2]=-wall->UR[1]*wall->ny+wall->UR[2]*wall->nx+wall->UR[2]*wall->nz;
	WL[2]=-wall->UL[1]*wall->ny+wall->UL[2]*wall->nx+wall->UL[2]*wall->nz;
      
      WR[3]=wall->UR[3]*wall->nx+wall->UR[3]*wall->ny-wall->UR[1]*wall->nz;
	WL[3]=wall->UL[3]*wall->nx+wall->UL[3]*wall->ny-wall->UL[1]*wall->nz;
      
	WR[4]=wall->UR[4];
	WL[4]=wall->UL[4];
	
#if MULTICOMPONENT
	WR[5]=wall->UR[5];
	WL[5]=wall->UL[5];
	phiL=WL[5]/WL[0];
	phiR=WR[5]/WR[0];
	#if MULTI_TYPE==1
		gammaL=phiL;
		gammaR=phiR;
	#else
		gammaL=1.0+1.0/phiL;
		gammaR=1.0+1.0/phiR;
	#endif
#else
	gammaL=_gamma_;
	gammaR=_gamma_;
#endif	
	
	/**Additional variables for the solver**/
	uL=WL[1]/WL[0];
	uR=WR[1]/WR[0];

	vL=WL[2]/WL[0];
	vR=WR[2]/WR[0];
      
      wL=WL[3]/WL[0];
	wR=WR[3]/WR[0];

	pL=(gammaL-1.0)*(WL[4]-0.5*WL[0]*(uL*uL+vL*vL+wL*wL));
	pR=(gammaR-1.0)*(WR[4]-0.5*WR[0]*(uR*uR+vR*vR+wR*wR));
	
	HL=(WL[4]+pL)/WL[0];
	HR=(WR[4]+pR)/WR[0];
	
	cL=sqrt(gammaL*pL/WL[0]);

	cR=sqrt(gammaR*pR/WR[0]);
	
	raizrhoL=sqrt(WL[0]);
	raizrhoR=sqrt(WR[0]);
	sumRaizRho=raizrhoR+raizrhoL;

	/**Hat variables (Roe averages)**/
	u_hat=(uR*raizrhoR+uL*raizrhoL)/sumRaizRho;
	v_hat=(vR*raizrhoR+vL*raizrhoL)/sumRaizRho;
      w_hat=(wR*raizrhoR+wL*raizrhoL)/sumRaizRho;
	H_hat=(HR*raizrhoR+HL*raizrhoL)/sumRaizRho;
#if MULTICOMPONENT	
	#if MULTI_TYPE==1
		gamma_hat=1.0+ 1.0/( (phiR*raizrhoR+phiL*raizrhoL)/sumRaizRho );
	#else
		gamma_hat=(gammaR*raizrhoR+gammaL*raizrhoL)/sumRaizRho;
	#endif
#else
	gamma_hat=_gamma_;
#endif

	c_hat=sqrt((gamma_hat-1)*(H_hat-0.5*(u_hat*u_hat+v_hat*v_hat+w_hat*w_hat)));

	/**Physical flux calculation (the F of the eqs.)**/

	FR[0]=WR[1];
	FL[0]=WL[1];

	FR[1]=WR[1]*uR+pR;
	FL[1]=WL[1]*uL+pL;

	FR[2]=WR[1]*vR;
	FL[2]=WL[1]*vL;
      
      FR[3]=WR[1]*wR;
	FL[3]=WL[1]*wL;

	FR[4]=uR*(WR[4]+pR);
	FL[4]=uL*(WL[4]+pL);


	/**Wave speed estimation**/
	
	//S1=MIN(uL-cL,u_hat-c_hat); //***************************-------------REVISAR!!!!!!!!!!!!!!!!!!!!!!!!
	//S2=MAX(uR+cR, u_hat+c_hat);
	S1=u_hat-c_hat; 
	S2=u_hat+c_hat;

	maxS=MAX(ABS(S1),ABS(S2));
	diffS=S2-S1;
	
	
	/**Source term**/
      
      pRe   =wall->pRe; //(gamma_hat-1.0)*wall->URe[4];
      pLe   =wall->pLe; //(gamma_hat-1.0)*wall->ULe[4];
      rhoRe = wall->URe[0];
      rhoLe = wall->ULe[0];
      
      
	S[0]=0.0;
      if(ABS(wall->nz)>TOL14){ //this is neccessary when considering atm pressures of 1.e5, to keep precision
            S[1]=(WR[0]+WL[0])*(pRe-pLe)/(rhoRe+rhoLe);
      }else{
            S[1]=0.0;
      }
	S[2]=0.0;
	S[3]=0.0;
	S[4]=S[1]*u_hat;
	
	psi=(rhoRe-rhoLe)*c_hat*c_hat/(pRe-pLe+TOL14);
	chi=0.5*(psi-1.0)*(v_hat*v_hat+w_hat*w_hat);
	
	B[0]=-psi*S[1]/(S1*S2);
	B[1]=0.0;
	B[2]=-psi*v_hat/(S1*S2)*S[1];
	B[3]=-psi*w_hat/(S1*S2)*S[1];
	B[4]=-(H_hat-u_hat*u_hat+chi)/(S1*S2)*S[1]; 
	
	/**HLLS flux calculation**/
	//WR[0]=0.0;
	//WL[0]=0.0;
	//fL_star
	for(m=0;m<5;m++){
		if(S1>=0){
			F_star[m]=FL[m];
		}else if(S2<=0){
			F_star[m]=FR[m]-S[m];
		}else{
                  F_star[m]=(S2*FL[m]-S1*FR[m]+S1*S2*(WR[m]-WL[m])+S1*(S[m]-S2*B[m]))/(diffS);
            }
	}	
	/**Inverse rotation of the flux**/
	wall->fL_star[0]=F_star[0]; //Mass is not vectorial
	wall->fL_star[1]=F_star[1]*wall->nx - F_star[2]*wall->ny - F_star[3]*wall->nz;
	wall->fL_star[2]=F_star[1]*wall->ny + F_star[2]*wall->nx + F_star[2]*wall->nz;
      wall->fL_star[3]=F_star[3]*wall->nx + F_star[3]*wall->ny + F_star[1]*wall->nz;
	wall->fL_star[4]=F_star[4]; //Energy is not vectorial
	
	//fR_star
	for(m=0;m<5;m++){
		if(S1>=0){
			F_star[m]=FL[m]+S[m];
		}else if(S2<=0){
			F_star[m]=FR[m];
		}else{
                  F_star[m]=(S2*FL[m]-S1*FR[m]+S1*S2*(WR[m]-WL[m])+S2*(S[m]-S1*B[m]))/(diffS);
            }
	}
	/**Inverse rotation of the flux**/
	wall->fR_star[0]=F_star[0]; //Mass is not vectorial
	wall->fR_star[1]=F_star[1]*wall->nx - F_star[2]*wall->ny - F_star[3]*wall->nz;
	wall->fR_star[2]=F_star[1]*wall->ny + F_star[2]*wall->nx + F_star[2]*wall->nz;
      wall->fR_star[3]=F_star[3]*wall->nx + F_star[3]*wall->ny + F_star[1]*wall->nz;
	wall->fR_star[4]=F_star[4]; //Energy is not vectorial
	
	/*if(ABS(S[1])>TOL8 ){
	printf(" nz: %14.14e\n",wall->nz);
	printf(" S1,S4: %14.14e %14.14e\n",S[1],S[4]);
      printf(" rho: %14.14e %14.14e %14.14e %14.14e\n",WR[0],WL[0],rhoRe,rhoLe);
	printf(" FL,FR: %14.14e %14.14e\n",FL[1],FR[1]);
	printf(" PLe,PRe: %14.14e %14.14e\n",pLe,pRe);
	printf(" PL,PR: %14.14e %14.14e\n",pL,pR);
	
	printf(" UR,UL: %14.14e %14.14e\n",wall->UR[1]/wall->UR[0],wall->UL[1]/wall->UL[0]);

	printf(" EL,ER: %14.14e %14.14e\n",WR[4],WL[4]);
	printf(" ELe,ERe: %14.14e %14.14e\n",wall->URe[4],wall->ULe[4]);
	printf(" rho/rhoE: %14.14e\n",(WR[0]+WL[0])/(rhoRe+rhoLe));
	printf(" deltaF, deltaP, delta %14.14e %14.14e  %14.14e \n",FR[1]-FL[1],pRe-pLe,FR[1]-FL[1]-(pRe-pLe) );
	
	printf(" FL: %14.14e %14.14e\n",wall->fL_star[3],FL[1]);
	printf(" FR: %14.14e %14.14e\n",wall->fR_star[3],FR[1]);
	printf(" dF: %14.14e %14.14e\n",(S2*FL[1]-S1*FR[1]+S2*S[1])-(S2-S1)*FR[1],   FR[1]-FL[1]-S[1]);
	printf(" dF (rho): %14.14e %14.14e\n",wall->fL_star[0]-FL[0],wall->fR_star[0]-FR[0]);
	printf(" Df-S: %14.14e\n",wall->fR_star[3]-wall->fL_star[3]-S[1]);
	printf(" Df-S: %14.14e\n",wall->fR_star[0]-wall->fL_star[0]);
	printf(" Df-S (rho): %14.14e\n",FR[0]-FL[0]-S[0]);
	printf(" Df-S (rho*u): %14.14e\n",FR[1]-FL[1]-S[1]);
	printf(" Df-S (rho*v): %14.14e\n",FR[2]-FL[2]-S[2]);
	printf(" Df-S (rho*w): %14.14e\n",FR[3]-FL[3]-S[3]);
	printf(" Df-S (E): %14.14e\n",FR[4]-FL[4]-S[4]);
	printf(" Du-B (rho): %14.14e %14.14e %14.14e %14.14e\n",WR[0],WL[0],B[0],WR[0]-WL[0]-B[0]);
	printf(" Du-B (rho*u): %14.14e \n",WR[1]-WL[1]-B[1]);
	printf(" Du-B (rho*v): %14.14e \n",WR[2]-WL[2]-B[2]);
	printf(" Du-B (rho*w): %14.14e \n",WR[3]-WL[3]-B[3]);
	printf(" Du-B (E): %14.14e \n",WR[4]-WL[4]-B[4]);
	getchar();
      }*/
      
      
	*lambda_max=MAX(*lambda_max,maxS);
		
}



void compute_transmissive_euler(t_wall *wall, int wp){

	int m;   
      double WR[6], WL[6]; /**<Auxiliar array of variables rotated for the 1D problem**/
	double uL, uR, vL, vR, wL, wR, pL, pR, HL, HR, cL, cR, gammaL, gammaR, phiL, phiR;
	double pLe, pRe;
	double FR[5], FL[5];
	double F_star[5];
      

	/**Rotation of array of variables**/
	
	WR[0]=wall->UR[0];
	WL[0]=wall->UL[0];

      // This is a simplification of the rotation matrix, only valid for cartesian mesh
	WR[1]=wall->UR[1]*wall->nx+wall->UR[2]*wall->ny+wall->UR[3]*wall->nz;
	WL[1]=wall->UL[1]*wall->nx+wall->UL[2]*wall->ny+wall->UL[3]*wall->nz;

	WR[2]=-wall->UR[1]*wall->ny+wall->UR[2]*wall->nx+wall->UR[2]*wall->nz;
	WL[2]=-wall->UL[1]*wall->ny+wall->UL[2]*wall->nx+wall->UL[2]*wall->nz;
      
      WR[3]=wall->UR[3]*wall->nx+wall->UR[3]*wall->ny-wall->UR[1]*wall->nz;
	WL[3]=wall->UL[3]*wall->nx+wall->UL[3]*wall->ny-wall->UL[1]*wall->nz;
      

	WR[4]=wall->UR[4];
	WL[4]=wall->UL[4];
	
#if MULTICOMPONENT
	WR[5]=wall->UR[5];
	WL[5]=wall->UL[5];
	phiL=WL[5]/WL[0];
	phiR=WR[5]/WR[0];
	#if MULTI_TYPE==1
		gammaL=phiL;
		gammaR=phiR;
	#else
	gammaL=1.0+1.0/phiL;
	gammaR=1.0+1.0/phiR;
	#endif
#else
	gammaL=_gamma_;
	gammaR=_gamma_;
#endif
	
	
	/**Additional variables for the solver**/
	uL=WL[1]/WL[0];
	uR=WR[1]/WR[0];

	vL=WL[2]/WL[0];
	vR=WR[2]/WR[0];
      
      wL=WL[3]/WL[0];
	wR=WR[3]/WR[0];

	pL=(gammaL-1.0)*(WL[4]-0.5*WL[0]*(uL*uL+vL*vL+wL*wL));
	pR=(gammaR-1.0)*(WR[4]-0.5*WR[0]*(uR*uR+vR*vR+wR*wR));
	
	

	/**Physical flux calculation (the F of the eqs.)**/    
      
      FR[0]=WR[1];
	FL[0]=WL[1];

#if ST==99
      pRe  =wall->pRe; 
      pLe  =wall->pLe; 
	FR[1]=WR[1]*uR+(pR-pRe);
	FL[1]=WL[1]*uL+(pL-pLe);
#else	
	FR[1]=WR[1]*uR+pR;
	FL[1]=WL[1]*uL+pL;
#endif

	FR[2]=WR[1]*vR;
	FL[2]=WL[1]*vL;
      
      FR[3]=WR[1]*wR;
	FL[3]=WL[1]*wL;

	FR[4]=uR*(WR[4]+pR);
	FL[4]=uL*(WL[4]+pL);


	/**the star flux is equal to the physical flux**/
	for(m=0;m<5;m++){
		if (wp==1 || wp==4 || wp==5){ //bottom and left interfaces, the innercell flux is FR
			F_star[m]=FR[m];
		}else{
			F_star[m]=FL[m];
		}
	}
	
      /**Inverse rotation of the flux, only valid for cartesian mesh**/
	wall->fR_star[0]=F_star[0]; //Mass is not vectorial
	wall->fR_star[1]=F_star[1]*wall->nx - F_star[2]*wall->ny - F_star[3]*wall->nz;
	wall->fR_star[2]=F_star[1]*wall->ny + F_star[2]*wall->nx + F_star[2]*wall->nz;
      wall->fR_star[3]=F_star[3]*wall->nx + F_star[3]*wall->ny + F_star[1]*wall->nz;
	wall->fR_star[4]=F_star[4]; //Energy is not vectorial
	
      for(m=0;m<5;m++){
            wall->fL_star[m]=wall->fR_star[m];
      }
      
      
		
}


void compute_solid_euler_hlle(t_wall *wall, double *lambda_max, int wp){

	int m;
	double WR[5], WL[5]; /**<Auxiliar array of variables rotated for the 1D problem**/
	double uL, uR, vL, vR, wL, wR, pL, pR, HL, HR, cL, cR;
	double pLe, pRe, rhoprimeL, rhoprimeR, EprimeL, EprimeR;
	double raizrhoR, raizrhoL, sumRaizRho;
	double u_hat, v_hat, w_hat, H_hat, c_hat;
	double S1, S2, diffS, maxS;
	double FR[5], FL[5];
	double F_star[5];

	/**Rotation of array of variables**/
	
	WR[0]=wall->UR[0];
	WL[0]=wall->UL[0];

      // This is a simplification of the rotation matrix, only valid for cartesian mesh
	WR[1]=wall->UR[1]*wall->nx+wall->UR[2]*wall->ny+wall->UR[3]*wall->nz;
	WL[1]=wall->UL[1]*wall->nx+wall->UL[2]*wall->ny+wall->UL[3]*wall->nz;

	WR[2]=-wall->UR[1]*wall->ny+wall->UR[2]*wall->nx+wall->UR[2]*wall->nz;
	WL[2]=-wall->UL[1]*wall->ny+wall->UL[2]*wall->nx+wall->UL[2]*wall->nz;
      
      WR[3]=wall->UR[3]*wall->nx+wall->UR[3]*wall->ny-wall->UR[1]*wall->nz;
	WL[3]=wall->UL[3]*wall->nx+wall->UL[3]*wall->ny-wall->UL[1]*wall->nz;
      


	WR[4]=wall->UR[4];
	WL[4]=wall->UL[4];
      
      
      for(m=0;m<5;m++){
		if (wp==1 || wp==4 || wp==5){ //bottom and left interfaces, the innercell state is WR, otherwise is WL			
                  if (m==1){
                        WL[m]=-WR[m];
                  }else{
                        WL[m]= WR[m];
                  }
		}else{
                  if (m==1){
                        WR[m]=-WL[m];
                  }else{
                        WR[m]= WL[m];
                  }
		}
	}
	
	#if ST==2||ST==3
	if (wp==1 || wp==4 || wp==5){ //bottom and left interfaces, the innercell state is WR, otherwise is WL			
		pRe = wall->pRe; 
		pLe = pRe;
		rhoprimeR=WR[0]-wall->URe[0];
		rhoprimeL=rhoprimeR;
		EprimeR=WR[4]-wall->URe[4];
		EprimeL=EprimeR;
	}else{
		pLe = wall->pLe; 
		pRe = pLe;
		EprimeL=WL[4]-wall->ULe[4];
		EprimeR=EprimeL;
		rhoprimeL=WL[0]-wall->ULe[0];
		rhoprimeR=rhoprimeL;
	}
	
	
	
      #endif
      
	
	/**Additional variables for the solver**/
	uL=WL[1]/WL[0];
	uR=WR[1]/WR[0];

	vL=WL[2]/WL[0];
	vR=WR[2]/WR[0];
      
      wL=WL[3]/WL[0];
	wR=WR[3]/WR[0];

	pL=pressure_from_energy(_gamma_, WL[4], uL, vL, wL, WL[0], wall->z);
	pR=pressure_from_energy(_gamma_, WR[4], uR, vR, wR, WR[0], wall->z);
	
#if ST==3	
	HL=(WL[4]-WL[0]*_g_*wall->z+pL)/WL[0];
	HR=(WR[4]-WR[0]*_g_*wall->z+pR)/WR[0];
#else
	HL=(WL[4]+pL)/WL[0];
	HR=(WR[4]+pR)/WR[0];
#endif
      
	
	cL=sqrt(_gamma_*pL/WL[0]);
	cR=sqrt(_gamma_*pR/WR[0]);
      
	
	raizrhoL=sqrt(WL[0]);
	raizrhoR=sqrt(WR[0]);
	sumRaizRho=raizrhoR+raizrhoL;

	/**Hat variables (Roe averages)**/
	u_hat=(uR*raizrhoR+uL*raizrhoL)/sumRaizRho;
	v_hat=(vR*raizrhoR+vL*raizrhoL)/sumRaizRho;
      w_hat=(wR*raizrhoR+wL*raizrhoL)/sumRaizRho;
	H_hat=(HR*raizrhoR+HL*raizrhoL)/sumRaizRho;

	c_hat=sqrt((_gamma_-1)*(H_hat-0.5*(u_hat*u_hat+v_hat*v_hat+w_hat*w_hat)));

	/**Physical flux calculation (the F of the eqs.)**/

	FR[0]=WR[1];
	FL[0]=WL[1];

#if ST==2||ST==3
	FR[1]=WR[1]*uR+(pR-pRe);
	FL[1]=WL[1]*uL+(pL-pLe);
#else	
	FR[1]=WR[1]*uR+pR;
	FL[1]=WL[1]*uL+pL;
#endif

	FR[2]=WR[1]*vR;
	FL[2]=WL[1]*vL;
      
      FR[3]=WR[1]*wR;
	FL[3]=WL[1]*wL;

	FR[4]=uR*(WR[4]+pR);
	FL[4]=uL*(WL[4]+pL);

#if ST==2||ST==3
	WL[0]=rhoprimeL;
	WR[0]=rhoprimeR;
	WL[4]=EprimeL;
	WR[4]=EprimeR;
#endif




	/**Wave speed estimation**/
	
	S1=MIN(uL-cL,u_hat-c_hat);
	S2=MAX(uR+cR, u_hat+c_hat);


	maxS=MAX(ABS(S1),ABS(S2));
	diffS=S2-S1;

	/**HLLE flux calculation**/
	for(m=0;m<5;m++){
		if(S1>=0){
			F_star[m]=FL[m];
		}else if(S2<=0){
			F_star[m]=FR[m];
		}else{
                  F_star[m]=(S2*FL[m]-S1*FR[m]+S1*S2*(WR[m]-WL[m]))/(diffS);
            }
	}
	
		//printf("%14.14e %14.14e \n",F_star[0],F_star[0]);
	
	/**Inverse rotation of the flux**/
	wall->fR_star[0]=F_star[0]; //Mass is not vectorial
	wall->fR_star[1]=F_star[1]*wall->nx - F_star[2]*wall->ny - F_star[3]*wall->nz;
	wall->fR_star[2]=F_star[1]*wall->ny + F_star[2]*wall->nx + F_star[2]*wall->nz;
      wall->fR_star[3]=F_star[3]*wall->nx + F_star[3]*wall->ny + F_star[1]*wall->nz;
	wall->fR_star[4]=F_star[4]; //Energy is not vectorial  
      //wall->fR_star[5]=0.0; //no solute flux
	
      for(m=0;m<5;m++){
            wall->fL_star[m]=wall->fR_star[m];
      }
      
      
	*lambda_max=MAX(*lambda_max,maxS);
		
}








void compute_euler_Roe(t_wall *wall,double *lambda_max){

	printf("%s We are working on it. Sorry!\n",WAR);
	abort();
}


void compute_sw_HLLE(t_wall *wal,double *lambda_maxl){

	printf("%s We are working on it. Sorry!\n",WAR);
	abort();


}



void compute_sw_Roe(t_wall *wall,double *lambda_max){

	printf("%s We are working on it. Sorry!\n",WAR);
	abort();

}


void compute_transport(t_wall *wall){

	if(wall->fR_star[0]<TOL14){ //negative transport -> information from right hand side
		wall->fR_star[5]=wall->fR_star[0]*wall->UR[5]/wall->UR[0];
		wall->fL_star[5]=wall->fR_star[5];
	}else{ //positive transport -> information from right hand side
		wall->fR_star[5]=wall->fL_star[0]*wall->UL[5]/wall->UL[0];
		wall->fL_star[5]=wall->fR_star[5];
	}
      
      //printf("%lf\n",wall->UL[5]);
      //printf("%lf\n",wall->UR[5]);
      
      //wall->fR_star[5]=0.0;
      //wall->fL_star[5]=0.0;

      
#if LINEAR_TRANSPORT
	wall->fR_star[1]=0.0;
	wall->fL_star[1]=0.0;
	wall->fR_star[2]=0.0;
	wall->fL_star[2]=0.0;
	wall->fR_star[3]=0.0;
	wall->fL_star[3]=0.0;
      wall->fR_star[4]=0.0;
	wall->fL_star[4]=0.0;
#endif

}



void compute_burgers_flux(t_wall *wall,double *lambda_max){
      
	double Savg,SL,SR;
      double fL,fR;
      double dU;
      
      fL=wall->UL[0]*wall->UL[0]/2.0;
      fR=wall->UR[0]*wall->UR[0]/2.0;
      
      Savg=(wall->UL[0]+wall->UR[0])*0.5;     
      dU=wall->UR[0]-wall->UL[0];
      
	wall->fR_star[0]=0.5*(fL+fR - ABS(Savg)*dU);
      wall->fL_star[0]=wall->fR_star[0];
      
      *lambda_max=MAX(*lambda_max,Savg);

}


void compute_linear_flux(t_wall *wall,double *lambda_max){
      
	double Savg,SL,SR;
      double fL,fR;
      double dU;
      
      Savg=wall->vel; 
      
      fL=wall->UL[0]*Savg;
      fR=wall->UR[0]*Savg;
       
      dU=wall->UR[0]-wall->UL[0];
      
	wall->fR_star[0]=0.5*(fL+fR - ABS(Savg)*dU);
      wall->fL_star[0]=wall->fR_star[0];
      
      *lambda_max=MAX(*lambda_max,Savg);

}

int update_dt(t_mesh *mesh,t_sim *sim){

	double dl;

	dl=MIN(mesh->dx,mesh->dy);
      dl=MIN(dl,mesh->dz);
	sim->dt=sim->CFL*dl/mesh->lambda_max;
	if(sim->dt+sim->t>sim->tf){
		sim->dt=sim->tf-sim->t+TOL14;
	}

      
      
	return 1;
}


void update_cell(t_mesh *mesh, t_sim *sim){
	
	int i,k;
	t_cell *cell;

	cell=mesh->cell;
	for(i=0;i<mesh->ncells;i++){
            if(cell->type!=0&&cell->ghost!=1){
		for(k=0;k<sim->nvar;k++){
			cell->U[k]-=sim->dt*((cell->w2->fL_star[k]-cell->w4->fR_star[k])/cell->dx + (cell->w3->fL_star[k]-cell->w1->fR_star[k])/cell->dy + (cell->w6->fL_star[k]-cell->w5->fR_star[k])/cell->dz );
		}
            //printf("%lf\n",cell->w2->fL_star[5]);
            if (cell->U[0]<TOL14){
            	printf("celda %d: rHO: %lf \n",i,cell->U[0]);
		getchar();
            }
            }
            
		cell++;
	}
}


void update_cellK1(t_mesh *mesh, t_sim *sim){
	
	int i,k;
	t_cell *cell;

	//cell=mesh->cell;
#pragma omp parallel for default(none) private(k,cell) shared(sim,mesh)
	for(i=0;i<mesh->ncells;i++){
		cell=&(mesh->cell[i]);
            if(cell->type!=0&&cell->ghost!=1){
		for(k=0;k<sim->nvar;k++){
			cell->U_aux[k]=cell->U[k];
			cell->U[k]-=sim->dt*((cell->w2->fL_star[k]-cell->w4->fR_star[k])/cell->dx + (cell->w3->fL_star[k]-cell->w1->fR_star[k])/cell->dy + (cell->w6->fL_star[k]-cell->w5->fR_star[k])/cell->dz - cell->S[k]);
		}
            }
	}
}


void update_cellK2(t_mesh *mesh, t_sim *sim){
	
	int i,k;
	t_cell *cell;

	//cell=mesh->cell;
#pragma omp parallel for default(none) private(k,cell) shared(sim,mesh)
	for(i=0;i<mesh->ncells;i++){
            cell=&(mesh->cell[i]);
            if(cell->type!=0&&cell->ghost!=1){
		for(k=0;k<sim->nvar;k++){
			cell->U[k]=0.75*cell->U_aux[k]+0.25*cell->U[k]-0.25*sim->dt*((cell->w2->fL_star[k]-cell->w4->fR_star[k])/cell->dx + (cell->w3->fL_star[k]-cell->w1->fR_star[k])/cell->dy + (cell->w6->fL_star[k]-cell->w5->fR_star[k])/cell->dz - cell->S[k]);

		}
            }
	}
}


void update_cellK3(t_mesh *mesh, t_sim *sim){
	
	int i,k;
	t_cell *cell;

	//cell=mesh->cell;
#pragma omp parallel for default(none) private(k,cell) shared(sim,mesh)    
	for(i=0;i<mesh->ncells;i++){
            cell=&(mesh->cell[i]);
            if(cell->type!=0&&cell->ghost!=1){
		for(k=0;k<sim->nvar;k++){
			cell->U[k]=(1.0/3.0)*cell->U_aux[k]+(2.0/3.0)*cell->U[k]-(2.0/3.0)*sim->dt*((cell->w2->fL_star[k]-cell->w4->fR_star[k])/cell->dx + (cell->w3->fL_star[k]-cell->w1->fR_star[k])/cell->dy + (cell->w6->fL_star[k]-cell->w5->fR_star[k])/cell->dz - cell->S[k]);

		}
		}
	}
}



void compute_source(t_mesh *mesh){
	
	int i;
	t_cell *cell;

#pragma omp parallel for default(none) private(cell) shared(mesh)
	for(i=0;i<mesh->ncells;i++){
		cell=&(mesh->cell[i]);
		#if ST==1
            if(cell->type!=0&&cell->st_sizeZ>1){     //This is the implementation of gravity force in -Z direction
			cell->S[3]= -_g_*cell->U[0] + cell->S_corr[3];
			cell->S[4]= -_g_*cell->U[3];
            }
		#elif ST==2
		if(cell->type!=0){     //This is the implementation of gravity force in -Z direction
			cell->S[3]= -_g_*(cell->U[0]-cell->Ue[0]);
			cell->S[4]= -_g_*cell->U[3];
            }
		#else
		if(cell->type!=0){     //This is the implementation of gravity force in -Z direction
			cell->S[3]= -_g_*(cell->U[0]-cell->Ue[0]);
			cell->S[4]= 0.0;
            }	
		#endif
	}
}



void mass_calculation(t_mesh *mesh, t_sim *sim){

	int i;
	double massAux;
	double area;

	area=mesh->dx*mesh->dy*mesh->dz;
	massAux=0.0;
#pragma omp parallel for default(none) shared(area,mesh) reduction(+:massAux)
	for(i=0;i<mesh->ncells;i++){
            if(mesh->cell[i].type!=0){
                  massAux+=mesh->cell[i].U[0]*area;
            }
	}
	mesh->mass=massAux;

}


void energy_calculation(t_mesh *mesh, t_sim *sim){

	int i;
	double energyAux;
	double area;

	area=mesh->dx*mesh->dy*mesh->dz;
	energyAux=0.0;
#pragma omp parallel for default(none) shared(area,mesh) reduction(+:energyAux)
	for(i=0;i<mesh->ncells;i++){
            if(mesh->cell[i].type!=0){
			#if ST==0||ST==3
			energyAux+=mesh->cell[i].U[4]*area;
			#else
                  energyAux+=(mesh->cell[i].U[4]+mesh->cell[i].U[0]*_g_*mesh->cell[i].zc)*area;
			#endif
            }
	}
	mesh->energy=energyAux;

}


void tke_calculation(t_mesh *mesh, t_sim *sim){

	int i;
	double tke_a,u,v,w;
	double volume,volumeT;

	volume=mesh->dx*mesh->dy*mesh->dz;
	volumeT=0.0;
	tke_a=0.0;
	for(i=0;i<mesh->ncells;i++){
            if(mesh->cell[i].type!=0){
                  u=mesh->cell[i].U[1]/mesh->cell[i].U[0];
                  v=mesh->cell[i].U[2]/mesh->cell[i].U[0];
                  w=mesh->cell[i].U[3]/mesh->cell[i].U[0];
                  tke_a+=0.5*mesh->cell[i].U[0]*(u*u + v*v + w*w)*volume;
				  volumeT+=volume;
            }
	}
	mesh->tke=tke_a/volumeT; //average TKE in the domain

}



////////////////////////////////////////////////////
/////  P O S T - P R O C.   F U N C T I O N S //////
////////////////////////////////////////////////////

int write_geo_vtk(t_mesh *mesh, char *filename){
		
		
	int i,j;
	FILE *fp;
	fp=fopen(filename,"w");

	// Write file header
	fprintf(fp,"# vtk DataFile Version 2.0\n");
	fprintf(fp,"Output file from Euler\n");
	fprintf(fp,"ASCII\n");
	fprintf(fp,"DATASET UNSTRUCTURED_GRID\n");

	// Write node coordinates
	fprintf(fp,"POINTS %d double \n",mesh->nnodes);
	for (i=0;i<mesh->nnodes;i++){
	   fprintf(fp,"%lf %lf %lf\n", mesh->node[i].x, mesh->node[i].y, mesh->node[i].z);
	}

	//Write cell-node connectivity
	fprintf(fp,"CELLS %d %d \n",mesh->ncells,mesh->ncells*(8+1));
	for(i=0;i<mesh->ncells;i++){
		fprintf(fp,"8 %d %d %d %d %d %d %d %d\n",mesh->cell[i].n1,mesh->cell[i].n2,mesh->cell[i].n3,mesh->cell[i].n4,mesh->cell[i].n5,mesh->cell[i].n6,mesh->cell[i].n7,mesh->cell[i].n8);
	}
	////////////////////////////////////////////////////
	////////////// C O N E C T I V I T Y ///////////////
	////////////////////////////////////////////////////

	//        (3)------------(4)
	//         |		  |
	//         |		  |
	//        (1)------------(2)
	//


	// Cell types
	fprintf(fp,"CELL_TYPES %d \n",mesh->ncells);
      	for(i=0;i<mesh->ncells;i++){
	   fprintf(fp,"%d \n",12);
	}
	
	//Cell data
	fprintf(fp,"CELL_DATA %d \n",mesh->ncells);

	//Water depth (h) difference
	
	fprintf(fp,"SCALARS stX INTEGER \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%d \n",mesh->cell[j].st_sizeX);
	}
	fprintf(fp,"SCALARS stY INTEGER \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%d \n",mesh->cell[j].st_sizeY);
	}
      fprintf(fp,"SCALARS stZ INTEGER \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%d \n",mesh->cell[j].st_sizeZ);
	}
      fprintf(fp,"SCALARS cellType INTEGER \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%d \n",mesh->cell[j].type);
	}
      fprintf(fp,"SCALARS ghostCell INTEGER \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%d \n",mesh->cell[j].ghost);
	}
	

	fclose(fp);
	printf("%s An VTK has been dumped: %s\n",OK,filename);


	return 1;
}


int write_vtk(t_mesh *mesh, char *filename){
		
		
	int i,j;
	FILE *fp;
	double gamma,theta,u,v,w;
	fp=fopen(filename,"w");

	// Write file header
	fprintf(fp,"# vtk DataFile Version 2.0\n");
	fprintf(fp,"Output file from Euler\n");
	fprintf(fp,"ASCII\n");
	fprintf(fp,"DATASET UNSTRUCTURED_GRID\n");

	// Write node coordinates
	fprintf(fp,"POINTS %d double \n",mesh->nnodes);
	for (i=0;i<mesh->nnodes;i++){
	   fprintf(fp,"%lf %lf %lf\n", mesh->node[i].x, mesh->node[i].y, mesh->node[i].z);
	}

	//Write cell-node connectivity
	fprintf(fp,"CELLS %d %d \n",mesh->ncells,mesh->ncells*(8+1));
	for(i=0;i<mesh->ncells;i++){
		fprintf(fp,"8 %d %d %d %d %d %d %d %d\n",mesh->cell[i].n1,mesh->cell[i].n2,mesh->cell[i].n3,mesh->cell[i].n4,mesh->cell[i].n5,mesh->cell[i].n6,mesh->cell[i].n7,mesh->cell[i].n8);
	}
	////////////////////////////////////////////////////
	////////////// C O N E C T I V I T Y ///////////////
	////////////////////////////////////////////////////

	//        (3)------------(4)
	//         |		  |
	//         |		  |
	//        (1)------------(2)
	//

	// Cell types
	fprintf(fp,"CELL_TYPES %d \n",mesh->ncells);
	for(i=0;i<mesh->ncells;i++){
	   fprintf(fp,"%d \n",12);
	}
	
	//Cell data
	fprintf(fp,"CELL_DATA %d \n",mesh->ncells);

	//Water depth (h) difference

      #if print_RHO	
	fprintf(fp,"SCALARS rho DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%14.14e\n",mesh->cell[j].U[0]);
	}
      #endif

#if EULER   
	for(j=0;j<mesh->ncells;j++){
		#if MULTICOMPONENT
			#if MULTI_TYPE==1
				gamma=mesh->cell[j].U[5]/mesh->cell[j].U[0];
			#else
				gamma=1.0+1.0/(mesh->cell[j].U[5]/mesh->cell[j].U[0]);
			#endif
		#else
			gamma=_gamma_;
		#endif
		u=mesh->cell[j].U[1]/mesh->cell[j].U[0];
		v=mesh->cell[j].U[2]/mesh->cell[j].U[0];
		w=mesh->cell[j].U[3]/mesh->cell[j].U[0];
            mesh->cell[j].pres=pressure_from_energy(gamma, mesh->cell[j].U[4], u, v, w, mesh->cell[j].U[0], mesh->cell[j].zc);
		//mesh->cell[j].pres=(gamma-1.0)*(mesh->cell[j].U[4]-0.5*mesh->cell[j].U[0]*(mesh->cell[j].U[1]*mesh->cell[j].U[1]+mesh->cell[j].U[2]*mesh->cell[j].U[2]+mesh->cell[j].U[3]*mesh->cell[j].U[3])/(mesh->cell[j].U[0]*mesh->cell[j].U[0]));
      }
      #if print_PRESSURE
	fprintf(fp,"SCALARS pres DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
      for(j=0;j<mesh->ncells;j++){
		fprintf(fp,"%14.14e \n",mesh->cell[j].pres);
      }
      #endif
      
      #if print_OVERPRESSURE
	fprintf(fp,"SCALARS d_pres DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%14.14e \n",mesh->cell[j].pres - mesh->cell[j].prese);
	}
      #endif
      
      #if print_VELOCITY
	fprintf(fp,"SCALARS U DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%14.14e \n",mesh->cell[j].U[1]/mesh->cell[j].U[0]);
	}

	fprintf(fp,"SCALARS V DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%14.14e \n",mesh->cell[j].U[2]/mesh->cell[j].U[0]);
	}

	fprintf(fp,"SCALARS W DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%14.14e \n",mesh->cell[j].U[3]/mesh->cell[j].U[0]);
	}
      #endif

      #if print_ENERGY
	fprintf(fp,"SCALARS E DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%14.14e \n",mesh->cell[j].U[4]);
	}
      #endif
      
      #if print_SOLUTES
	fprintf(fp,"SCALARS phi DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
	   	fprintf(fp,"%14.14e \n",mesh->cell[j].U[5]);
	}
      #endif
      
      #if print_POTENTIALTEM   
	fprintf(fp,"SCALARS theta DOUBLE \n");
	fprintf(fp,"LOOKUP_TABLE DEFAULT \n");
	for(j=0;j<mesh->ncells;j++){
            theta=mesh->cell[j].pres/(_R_*mesh->cell[j].U[0])/( pow((mesh->cell[j].pres/_p0_),((_gamma_-1.0)/_gamma_)) );
	   	fprintf(fp,"%14.14e \n",theta);
	}
      #endif
      
      
#endif

	fclose(fp);
	printf("%s An VTK has been dumped: %s\n",OK,filename);


	return 1;
}





int write_matrix_u(t_mesh *mesh, char *filename){
		
		
	int i,j,k;
      double u,v;
	FILE *fp;
	fp=fopen(filename,"w");

	// Write file header

	for(j=0;j<mesh->ycells;j++){
            for(i=0;i<mesh->xcells;i++){                  
                  k=i+j*mesh->xcells;                 
                  u=mesh->cell[k].U[1]/mesh->cell[k].U[0];
                  v=mesh->cell[k].U[2]/mesh->cell[k].U[0];
                  fprintf(fp,"%14.14e    ",u);
            }
            fprintf(fp,"\n");
	}

	fclose(fp);


	return 1;
}

int write_matrix_v(t_mesh *mesh, char *filename){
		
		
	int i,j,k;
      double u,v;
	FILE *fp;
	fp=fopen(filename,"w");

	// Write file header

	for(j=0;j<mesh->ycells;j++){
            for(i=0;i<mesh->xcells;i++){                  
                  k=i+j*mesh->xcells;                 
                  u=mesh->cell[k].U[1]/mesh->cell[k].U[0];
                  v=mesh->cell[k].U[2]/mesh->cell[k].U[0];
                  fprintf(fp,"%14.14e   ",v);
            }
            fprintf(fp,"\n");
	}

	fclose(fp);


	return 1;
}




int write_list(t_mesh *mesh, char *filename){
		
		
	int l,m,n,k;
	double u,v,w,p,rho,phi,gamma,theta;
	FILE *fp;
	fp=fopen(filename,"w");

	// Write file header
	fprintf(fp,"VARIABLES = X, Y, Z, u, v, w, rho, p, phi, theta \n");
	fprintf(fp,"CELLS = %d, %d, %d,\n",mesh->xcells,mesh->ycells,mesh->zcells);

    for(l=0;l<mesh->xcells;l++){  
		for(m=0;m<mesh->ycells;m++){
			for(n=0;n<mesh->zcells;n++){
			k = l + m*mesh->xcells + n*mesh->xcells*mesh->ycells;
			u=mesh->cell[k].U[1]/mesh->cell[k].U[0];
			v=mesh->cell[k].U[2]/mesh->cell[k].U[0];
			w=mesh->cell[k].U[3]/mesh->cell[k].U[0];
			rho=mesh->cell[k].U[0];
			phi=mesh->cell[k].U[5]/mesh->cell[k].U[0];
			#if MULTICOMPONENT
				#if MULTI_TYPE==1
					gamma=phi;
				#else
					gamma=1.0+1.0/phi;
				#endif
			#else
				gamma=_gamma_;
			#endif
			//p=(gamma-1.0)*(mesh->cell[k].U[4]-0.5*mesh->cell[k].U[0]*(mesh->cell[k].U[1]*mesh->cell[k].U[1]+mesh->cell[k].U[2]*mesh->cell[k].U[2]+mesh->cell[k].U[3]*mesh->cell[k].U[3])/(mesh->cell[k].U[0]*mesh->cell[k].U[0]));
			p=pressure_from_energy(gamma, mesh->cell[k].U[4], u, v, w, mesh->cell[k].U[0], mesh->cell[k].zc);
			theta=p/(_R_*mesh->cell[k].U[0])/( pow((p/_p0_),((_gamma_-1.0)/_gamma_)) );
                  fprintf(fp,"%14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e\n",mesh->cell[k].xc,mesh->cell[k].yc,mesh->cell[k].zc,u,v,w,rho,p,phi,theta);
                  //fprintf(fp,"%14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e\n",u,u,u,u,v,u,u,u,phi,theta);
            }
		}
	}

	fclose(fp);


	return 1;
}


int write_list_eq(t_mesh *mesh, char *filename){
		
		
	int l,m,n,k;
	double u,v,w,p,rho,phi,gamma,theta;
	FILE *fp;
	fp=fopen(filename,"w");

	// Write file header
	fprintf(fp,"VARIABLES = X, Y, Z, u, v, w, rho, p, phi, theta \n");
	fprintf(fp,"CELLS = %d, %d, %d,\n",mesh->xcells,mesh->ycells,mesh->zcells);

    for(l=0;l<mesh->xcells;l++){  
		for(m=0;m<mesh->ycells;m++){
			for(n=0;n<mesh->zcells;n++){
			k = l + m*mesh->xcells + n*mesh->xcells*mesh->ycells;
			u=mesh->cell[k].Ue[1]/mesh->cell[k].Ue[0];
			v=mesh->cell[k].Ue[2]/mesh->cell[k].Ue[0];
			w=mesh->cell[k].Ue[3]/mesh->cell[k].Ue[0];
			rho=mesh->cell[k].Ue[0];
			phi=mesh->cell[k].Ue[5]/mesh->cell[k].Ue[0];
			#if MULTICOMPONENT
				#if MULTI_TYPE==1
					gamma=phi;
				#else
					gamma=1.0+1.0/phi;
				#endif
			#else
				gamma=_gamma_;
			#endif
			//p=(gamma-1.0)*(mesh->cell[k].Ue[4]-0.5*mesh->cell[k].Ue[0]*(mesh->cell[k].Ue[1]*mesh->cell[k].Ue[1]+mesh->cell[k].Ue[2]*mesh->cell[k].Ue[2]+mesh->cell[k].Ue[3]*mesh->cell[k].Ue[3])/(mesh->cell[k].Ue[0]*mesh->cell[k].Ue[0]));
			p=pressure_from_energy(gamma, mesh->cell[k].Ue[4], u, v, w, mesh->cell[k].Ue[0], mesh->cell[k].zc);
			theta=p/(_R_*mesh->cell[k].Ue[0])/( pow((p/_p0_),((_gamma_-1.0)/_gamma_)) );
                  fprintf(fp,"%14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e %14.14e \n",mesh->cell[k].xc,mesh->cell[k].yc,mesh->cell[k].zc,u,v,w,rho,p,phi,theta);
            }
		}
	}

	fclose(fp);


	return 1;
}




int read_solids(t_mesh *mesh,t_solid *solids ){
		
	int i,j,k;
	FILE *fp;
	char fname[1024];
	char buffer[256];
      double aux1,aux2,aux3,aux4,f;
      t_triangle *triangle;
      t_stl *stl;
	
      f=1.0-TOL14;
	
	sprintf(fname,"case/solid_list.txt");
	fp = fopen(fname,"r");
	
	if (fp == NULL)
	{
		printf("%s No solids are found in folder case/.. \n",WAR);
		solids->nsolid=0;
	}else{

	fscanf(fp,"%*s %d", &solids->nsolid);
	printf(" The number of solids domains is: %d \n",solids->nsolid);
		
	for(i=0;i<solids->nsolid;i++)
	{
	  fscanf(fp,"%s", buffer);
	  solids->filename[i] = strdup(buffer);
	  printf(" Solid %d is located at %s \n",i, solids->filename[i]);
	}
	
	printf("\n");	
	fclose(fp);
	
	
	solids->stl=(t_stl*)malloc(solids->nsolid*sizeof(t_stl));
	stl=solids->stl;
      
	for(i=0;i<solids->nsolid;i++){
		strncpy(solids->stl[i].name, solids->filename[i],256);
		fp = fopen(solids->stl[i].name,"r");
		fscanf(fp,"%*s %d", &solids->stl[i].ntri);
            fscanf(fp,"%*s %d", &solids->stl[i].nver);
		printf(" Solid %d (%s) has %d facets and %d vertices \n",i, solids->filename[i],solids->stl[i].ntri,solids->stl[i].nver);
            
            solids->stl[i].triangle=(t_triangle*)malloc(solids->stl[i].ntri*sizeof(t_triangle));
            triangle=solids->stl[i].triangle;
            
            
            for(k=0;k<3;k++){
                  stl[i].Xmin[k]= 9999999999999.0; 
                  stl[i].Xmax[k]=-9999999999999.0;                    
            }

            
            for(j=0;j<solids->stl[i].ntri;j++){
                  fscanf(fp,"%lf %lf %lf", &triangle[j].nr[0],&triangle[j].nr[1],&triangle[j].nr[2]); // x, y and z components of the triangle normal are read
                  fscanf(fp,"%lf %lf %lf", &triangle[j].p1[0],&triangle[j].p1[1],&triangle[j].p1[2]); // x, y and z components of the triangle P1 node are read
                  fscanf(fp,"%lf %lf %lf", &triangle[j].p2[0],&triangle[j].p2[1],&triangle[j].p2[2]); // x, y and z components of the triangle P2 node are read
                  fscanf(fp,"%lf %lf %lf", &triangle[j].p3[0],&triangle[j].p3[1],&triangle[j].p3[2]); // x, y and z components of the triangle P3 node are read
                  //printf("%lf %lf %lf\n", triangle[j].nr[0],triangle[j].nr[1],triangle[j].nr[2]);
                  //printf("%lf %lf %lf\n", triangle[j].p1[0],triangle[j].p1[1],triangle[j].p1[2]);
                  //printf("%lf %lf %lf\n", triangle[j].p2[0],triangle[j].p2[1],triangle[j].p2[2]);
                  //printf("%lf %lf %lf\n", triangle[j].p3[0],triangle[j].p3[1],triangle[j].p3[2]);
                  triangle[j].absnr=pow(triangle[j].nr[0]*triangle[j].nr[0] + triangle[j].nr[1]*triangle[j].nr[1] + triangle[j].nr[2]*triangle[j].nr[2]  ,  0.5);
                  triangle[j].outside=0;
                  for(k=0;k<3;k++){
                        aux1=MIN(triangle[j].p1[k],triangle[j].p2[k]);
                        aux2=MIN(aux1,triangle[j].p3[k]);
                        stl[i].Xmin[k]=MIN(aux2,stl[i].Xmin[k]);         
                        
                        aux3=MAX(triangle[j].p1[k],triangle[j].p2[k]);
                        aux4=MAX(aux3,triangle[j].p3[k]);
                        stl[i].Xmax[k]=MAX(aux4,stl[i].Xmax[k]);
                        
                        
                        
                        if(k==0){
                              triangle[j].imin[k]= aux2/mesh->dx;// DEBERIAMOS CONSIDERAR UN dx/2 PARA CENTRARLO BIEN?????????????????????????????????????????
                              triangle[j].imax[k]= aux4/mesh->dx;
                              if(triangle[j].imax[k]-triangle[j].imin[k] < MAX(mesh->sim->order-1,1)){
                                    triangle[j].imin[k] = (aux2+aux4)/(2.0*mesh->dx) - (mesh->sim->order-1)/2;
                                    triangle[j].imax[k] = triangle[j].imin[k] + mesh->sim->order;
                                    //printf("triangle %d: dif %d   \n\n", j,triangle[j].imax[k]-triangle[j].imin[k]);
                                    //printf("triangle %d: %d %d  \n\n", j,triangle[j].imin[k],triangle[j].imax[k]);
                              }                              
                              triangle[j].imin[k]=MAX(triangle[j].imin[k],0);                   // to do: poner flag si el triangulo esta fuera, para sacarlo del calculo, y quitar esos min/max. Poner una minima separacionm entre imin/imax
                              triangle[j].imin[k]=MIN(triangle[j].imin[k],mesh->xcells-1);
                              triangle[j].imax[k]=MAX(triangle[j].imax[k],0);
                              triangle[j].imax[k]=MIN(triangle[j].imax[k],mesh->xcells-1);
                              if(aux2<0.0||aux4>mesh->Lx){
                                    triangle[j].outside=1;
                              }
                              //printf("triangle %d: %lf %lf  \n", j,aux2,aux4);
                              //printf("triangle %d: %d %d  \n\n", j,triangle[j].imin[k],triangle[j].imax[k]);
                        }else if(k==1){
                              triangle[j].imin[k]= aux2/mesh->dy;
                              triangle[j].imax[k]= aux4/mesh->dy;
                              if(triangle[j].imax[k]-triangle[j].imin[k] < MAX(mesh->sim->order-1,1)){
                                    triangle[j].imin[k] = (aux2+aux4)/(2.0*mesh->dx) - (mesh->sim->order-1)/2;
                                    triangle[j].imax[k] = triangle[j].imin[k] + mesh->sim->order;
                              }
                              triangle[j].imin[k]=MAX(triangle[j].imin[k],0);
                              triangle[j].imin[k]=MIN(triangle[j].imin[k],mesh->ycells-1);
                              triangle[j].imax[k]=MAX(triangle[j].imax[k],0);
                              triangle[j].imax[k]=MIN(triangle[j].imax[k],mesh->ycells-1);
                              if(aux2<0.0||aux4>mesh->Ly){
                                    triangle[j].outside=1;
                              }
                              //printf("triangle %d: %lf %lf  \n", j,aux2,aux4);
                              //printf("triangle %d: %d %d  \n", j,triangle[j].imin[k],triangle[j].imax[k]);
                        }else{
                              triangle[j].imin[k]= aux2/mesh->dz;
                              triangle[j].imax[k]= aux4/mesh->dz;
                              if(triangle[j].imax[k]-triangle[j].imin[k] < MAX(mesh->sim->order-1,1)){
                                    triangle[j].imin[k] = (aux2+aux4)/(2.0*mesh->dx) - (mesh->sim->order-1)/2;
                                    triangle[j].imax[k] = triangle[j].imin[k] + mesh->sim->order;
                              }
                              triangle[j].imin[k]=MAX(triangle[j].imin[k],0);
                              triangle[j].imin[k]=MIN(triangle[j].imin[k],mesh->zcells-1);
                              triangle[j].imax[k]=MAX(triangle[j].imax[k],0);
                              triangle[j].imax[k]=MIN(triangle[j].imax[k],mesh->zcells-1);
                              if(aux2<0.0||aux4>mesh->Lz){
                                    triangle[j].outside=1;
                              }
                              //printf("triangle %d: %lf %lf  \n", j,aux2,aux4);
                              //printf("triangle %d: %d %d  \n", j,triangle[j].imin[k],triangle[j].imax[k]);
                        }
                        //triangle[j].imin[k]=(int) 
                        
                  }
                  //printf("triangle %d: %d \n", j,triangle[j].outside);
            
            }
            //getchar();
            
            for(k=0;k<3;k++){
                  if(k==0){
                        stl[i].imin[k]= stl[i].Xmin[k]/mesh->dx;
                        stl[i].imax[k]= stl[i].Xmax[k]/mesh->dx;
                        stl[i].imin[k]=MAX(stl[i].imin[k],0);
                        stl[i].imin[k]=MIN(stl[i].imin[k],mesh->xcells-1);
                        stl[i].imax[k]=MAX(stl[i].imax[k],0);
                        stl[i].imax[k]=MIN(stl[i].imax[k],mesh->xcells-1);
                        //printf("stl %d: %lf %lf  \n", i,aux2,aux4);
                        //printf("stl %d: %d %d  \n", i,stl[i].imin[k],stl[i].imax[k]);
                  }else if(k==1){
                        stl[i].imin[k]= stl[i].Xmin[k]/mesh->dy;
                        stl[i].imax[k]= stl[i].Xmax[k]/mesh->dy;
                        stl[i].imin[k]=MAX(stl[i].imin[k],0);
                        stl[i].imin[k]=MIN(stl[i].imin[k],mesh->ycells-1);
                        stl[i].imax[k]=MAX(stl[i].imax[k],0);
                        stl[i].imax[k]=MIN(stl[i].imax[k],mesh->ycells-1);
                        //printf("stl %d: %lf %lf  \n", i,aux2,aux4);
                        //printf("stl %d: %d %d  \n", i,stl[i].imin[k],stl[i].imax[k]);
                  }else{
                        stl[i].imin[k]= stl[i].Xmin[k]/mesh->dz;
                        stl[i].imax[k]= stl[i].Xmax[k]/mesh->dz;
                        stl[i].imin[k]=MAX(stl[i].imin[k],0);
                        stl[i].imin[k]=MIN(stl[i].imin[k],mesh->zcells-1);
                        stl[i].imax[k]=MAX(stl[i].imax[k],0);
                        stl[i].imax[k]=MIN(stl[i].imax[k],mesh->zcells-1);
                        //printf("stl %d: %lf %lf  \n", i,aux2,aux4);
                        //printf("stl %d: %d %d  \n", i,stl[i].imin[k],stl[i].imax[k]);
                  }
            }
            printf(" The bounding box of solid %d is: \n (x,y,z)_min=(%lf,%lf,%lf)\n (x,y,z)_max=(%lf,%lf,%lf) \n", i,solids->stl[i].Xmin[0],solids->stl[i].Xmin[1],solids->stl[i].Xmin[2],solids->stl[i].Xmax[0],solids->stl[i].Xmax[1],solids->stl[i].Xmax[2]);
            printf(" and the respective indices are:  \n (i,j,k)_min=(%d,%d,%d)\n (i,j,k)_max=(%d,%d,%d) \n", solids->stl[i].imin[0],solids->stl[i].imin[1],solids->stl[i].imin[2],solids->stl[i].imax[0],solids->stl[i].imax[1],solids->stl[i].imax[2]);
		fclose(fp);
		printf("\n");	
	}
      
      printf("%s Solid domains have been successfully read\n",OK);
      
	/*triangle=solids->stl[1].triangle;
      for(j=0;j<solids->stl[1].ntri;j++){
      
	printf("%lf \n", triangle[j].nr[1]);
	}*/


	}
	return 1;
}


void print_info(t_mesh *mesh, t_sim *sim){

      printf("\n\e[94mAuthors:\n - Adrián Navas Montilla\n - Isabel Echeverribar \n");
      printf("Copyright (C) 2018-2019 The authors.   \n\n");     
      printf("License type: Creative Commons Attribution-NonCommercial-NoDerivs 3.0 Spain (CC BY-NC-ND 3.0 ES under the following terms: \n \n - Attribution — You must give appropriate credit and provide a link to the license. \n - NonCommercial — You may not use the material for commercial purposes. \n - NoDerivatives — If you remix, transform, or build upon the material, you may not distribute the modified material unless explicit permission of the authors is provided.\n\n");     
      printf("Disclaimer: This software is distributed for research and/or academic purposes, WITHOUT ANY WARRANTY. In no event shall the authors be liable for any claim, damages or other liability, arising from, out of or in connection with the software or the use or other dealings in this software.\e[0m\n");
      
		
      printf(" \n");
      printf(" \e[4mSIMULATION SETUP:\e[0m\n");
#if TYPE_REC==0
	printf(" WENO reconstruction of order %d is chosen. \n",sim->order);
#endif
#if TYPE_REC==1
	printf(" TENO reconstruction of order %d is chosen. \n",sim->order);
#endif
#if TYPE_REC==2
	printf("%s UWC (optimal weights) reconstruction of order %d is chosen. \n",WAR,sim->order);
#endif
      printf(" Final time: %lf\n",sim->tf);
      printf(" CFL: %lf\n",sim->CFL);
      printf(" Number of cells X: %d\n",mesh->xcells);
      printf(" Number of cells Y: %d\n",mesh->ycells);
      printf(" Number of cells Z: %d\n",mesh->zcells);
      printf(" Domain size: %lf x %lf x %lf \n",mesh->Lx,mesh->Ly,mesh->Lz);
      printf(" Boundaries (1: periodic, 2: user defined, 3: transmissive, 4: solid wall): \n"); // 1: periodic, 2: user defined, 3: transmissive, 4: solid wall
      printf(" Face_1(-y): %d \n",mesh->bc[0]);
      printf(" Face_2(+x): %d \n",mesh->bc[1]);
      printf(" Face_3(+y): %d \n",mesh->bc[2]);
      printf(" Face_4(-x): %d \n",mesh->bc[3]);
      printf(" Face_5(-z): %d \n",mesh->bc[4]);
      printf(" Face_6(+z): %d \n",mesh->bc[5]);
#if LINEAR_TRANSPORT||LINEAR
      printf("%s LINEAR TRANPORT IS ACTIVE. \n",WAR);
      printf(" Linear transport velocity: \n"); 
      printf(" u_x: %lf \n",mesh->u_x);
      printf(" u_y: %lf \n",mesh->u_y);
      printf(" u_z: %lf \n",mesh->u_z);
#endif
      
      printf(" \n");
      
      printf("%s Configuration file has been read \n",OK);

#if ST!=1&&HLLS==1
	printf("%s HLLS solver cannot be used when ST=0 or ST=2. Please use HLLE or HLLC. Press any key to exit... \n",ERR);
	getchar();
	exit(1);
#endif

#if HLLE+HLLS+ROE+HLLC > 1 
	printf("%s More than one solver is selected, please choose only one. Press any key to exit... \n",ERR);
	getchar();
	exit(1);
#endif
      
      
      if((mesh->bc[1]==1 && mesh->bc[3]!=1)||(mesh->bc[1]!=1 && mesh->bc[3]==1)){
            printf("%s Cyclic BC in X not properly set, only one of the boundaries is set as cyclic. The program will close when pressing a key. \n",ERR);
            getchar();
            exit(1);
	}
      if((mesh->bc[0]==1 && mesh->bc[2]!=1)||(mesh->bc[0]!=1 && mesh->bc[2]==1)){
            printf("%s Cyclic BC in Y not properly set, only one of the boundaries is set as cyclic. The program will close when pressing a key. \n",ERR);
            getchar();
            exit(1);
	}
      if((mesh->bc[4]==1 && mesh->bc[5]!=1)||(mesh->bc[4]!=1 && mesh->bc[5]==1)){
            printf("%s Cyclic BC in Z not properly set, only one of the boundaries is set as cyclic. The program will close when pressing a key. \n",ERR);
            getchar();
            exit(1);
	}
      
      
      if(mesh->bc[1]==1 && mesh->xcells<=(sim->order-1)/2){
            printf("%s The number of cells in X is too small for periodic BC. Transmissive BC are considered instead. \n",WAR);
            mesh->bc[1]=3;
            mesh->bc[3]=3;

	}
      if(mesh->bc[0]==1 && mesh->ycells<=(sim->order-1)/2){
            printf("%s The number of cells in Y is too small for periodic BC. Transmissive BC are considered instead. \n",WAR);
            mesh->bc[0]=3;
            mesh->bc[2]=3;

	}
      if(mesh->bc[4]==1 && mesh->zcells<=(sim->order-1)/2){
            printf("%s The number of cells in Z is too small for periodic BC. Transmissive BC are considered instead. \n",WAR);
            mesh->bc[4]=3;
            mesh->bc[5]=3;

	}

}


double energy_from_pressure(double gm, double p, double u, double v, double w, double rho, double z){

	double energy;
	
	#if ST==3
	energy=p/(gm-1.0)+0.5*rho*(u*u + v*v + w*w)+rho*_g_*z;
	#else
	energy=p/(gm-1.0)+0.5*rho*(u*u + v*v + w*w);
	#endif

	return energy;
}

double pressure_from_energy(double gm, double E, double u, double v, double w, double rho, double z){

	double pressure;
	
	#if ST==3
	pressure=(gm-1.0)*(E-0.5*rho*(u*u + v*v + w*w)-rho*_g_*z);
	#else
	pressure=(gm-1.0)*(E-0.5*rho*(u*u + v*v + w*w));
	#endif
	

	return pressure;
}





////////////////////////////////////////////////////
//////////////////// M A I N ///////////////////////
////////////////////////////////////////////////////

int main(int argc, char * argv[]){
	
	int i, j, k, p, m, n;
	t_mesh *mesh;
	t_sim *sim;
	t_solid *solids;
	char vtkfile[1024];
	char listfile[1024];
      char matrixfile_u[1024];
	char matrixfile_v[1024];
      char matrixfile_phi[1024];
      char fname[1024];
	double tf,t;
	int nIt; //counter for iterations
	double timeac; //accumulated time before dump
	double timeac2,tTke; //other counter. dump time for tke
	double aux1,aux2;
	double mass0,energy0;
	t_cell *cell;
      FILE *file_input;
      FILE *file_tke;
      
#ifdef _OPENMP 
	omp_set_num_threads(NTHREADS);
     	printf("The number of threads is set to %d.\n",NTHREADS);
#pragma omp parallel
     printf("Hello from thread %d of %d.\n", omp_get_thread_num(), omp_get_num_threads());

#endif



      
      ////////////////////////////////////////////////////
	///////////// M E M O R Y  A L L O C. //////////////
	////////////////////////////////////////////////////

	//Mesh allocation
	mesh=(t_mesh*)malloc(sizeof(t_mesh));

	//Simulation allocation
	sim=(t_sim*)malloc(sizeof(t_sim));

	//Solids allocation
	solids=(t_solid*)malloc(sizeof(t_solid));
	
	
	////////////////////////////////////////////////////
	////////////// P R E - P R O C E S S ///////////////
	////////////////////////////////////////////////////
	//
	// S I M U L A T I O N   P A R A M E T E R S

      sprintf(fname,"case/configure.input");	
      file_input = fopen(fname,"r"); // "r"= only read
      fscanf(file_input,"%*s");  
      fscanf(file_input,"%*s %lf" ,&sim->tf);  
      fscanf(file_input,"%*s %lf" ,&sim->tVolc);     
      fscanf(file_input,"%*s %lf" ,&sim->CFL);  
      fscanf(file_input,"%*s %d" ,&sim->order); 
      fscanf(file_input,"%*s");   
      fscanf(file_input,"%*s %d" ,&mesh->xcells); 
      fscanf(file_input,"%*s %d" ,&mesh->ycells); 
      fscanf(file_input,"%*s %d" ,&mesh->zcells);
      fscanf(file_input,"%*s %lf" ,&mesh->Lx); 
      fscanf(file_input,"%*s %lf" ,&mesh->Ly); 
      fscanf(file_input,"%*s %lf" ,&mesh->Lz); 
      fscanf(file_input,"%*s"); 
      fscanf(file_input,"%*s %d" ,&mesh->bc[0]);    
      fscanf(file_input,"%*s %d" ,&mesh->bc[1]); 
      fscanf(file_input,"%*s %d" ,&mesh->bc[2]); 
      fscanf(file_input,"%*s %d" ,&mesh->bc[3]); 
      fscanf(file_input,"%*s %d" ,&mesh->bc[4]); 
      fscanf(file_input,"%*s %d" ,&mesh->bc[5]);  
      fscanf(file_input,"%*s");   
      fscanf(file_input,"%*s %lf" ,&mesh->u_x);  
      fscanf(file_input,"%*s %lf" ,&mesh->u_y);  
      fscanf(file_input,"%*s %lf" ,&mesh->u_z);        
      fclose(file_input);    
	

#if EULER
	sim->nvar=6;
#endif
#if BURGERS
	sim->nvar=1;
#endif
#if LINEAR
	sim->nvar=1;
#endif

	// M E S H   P A R A M E T E R S
	mesh->dx= mesh->Lx/mesh->xcells;
	mesh->dy= mesh->Ly/mesh->ycells;
      mesh->dz= mesh->Lz/mesh->zcells;
          
      print_info(mesh,sim);
        
      //Initialization of ac. dump time
	timeac=0.0;
      timeac2=0.0;
      tTke=0.05;
		
	// S I M U L A T I O N  S E T - U P
	if(sim->order==1){
		sim->rk_steps=1;
	}else{
		sim->rk_steps=3;
	}
	
	// S E T  F U N C T I O N S
	create_mesh(mesh,sim);
      printf("%s Memory has been allocated and mesh connectivity has been defined \n\n",OK);
	#if ALLOW_SOLIDS
	read_solids(mesh,solids);
	#else
	solids->nsolid=0;
	#endif
      assign_cell_type(mesh,solids);
      if(solids->nsolid>0){
      printf("%s Ghost and solid cells have been identified \n",OK);}
      update_stencils(mesh,sim);
      assign_wall_type(mesh);    //esto da problemas raros -> valgrind?
	update_initial(mesh,sim);
      
      
      #if ALLOW_SOLIDS
      assign_image_cells(mesh,solids);
      update_ghost_cells(sim,mesh,solids);
      update_wall_type(mesh,solids);
      printf("%s Image points have been defined and ghost cell values have been computed \n",OK);
      #endif
	  

      
	#if LINEAR_TRANSPORT == 1
		set_velocity_field(mesh,sim);
	#endif
      #if LINEAR
		set_velocity(mesh,sim);
	#endif
//	//update_boundaries(mesh,bc); //??
	
	// M E S H   C R E A T I O N   D E B U G G I N G
	write_geo_vtk(mesh,"output-files/inital_geo_mesh.vtk");
	write_vtk(mesh,"output-files/state000.vtk");
	write_list(mesh,"output-files/list000.out");
	write_list_eq(mesh,"output-files/list_eq.out");
      printf("\n");
      printf(" T= 0.0e+0, Initial data printed. Starting time loop.\n\n");
//    write_matrix_u(mesh,"output-files/u0.dat");
//	write_matrix_v(mesh,"output-files/v0.dat");
	#if DEBUG_MESH
		for(i=0;i<mesh->ncells;i++){
	      
		    printf("Cell %d \n",i);
		    printf(" w1_id %d, w2_id %d, w3_id %d, w4_id %d, w5_id %d, w6_id %d \n",mesh->cell[i].w1_id,mesh->cell[i].w2_id,mesh->cell[i].w3_id,mesh->cell[i].w4_id,mesh->cell[i].w5_id,mesh->cell[i].w6_id);
		    printf(" n1 %d, n2 %d, n3 %d, n4 %d, n5 %d, n6 %d, n7 %d, n8 %d  \n",mesh->cell[i].n1,mesh->cell[i].n2,mesh->cell[i].n3,mesh->cell[i].n4,mesh->cell[i].n5,mesh->cell[i].n6,mesh->cell[i].n7,mesh->cell[i].n8);
		    printf(" Cell center (x,y,z)=(%lf, %lf, %lf) \n",mesh->cell[i].xc,mesh->cell[i].yc,mesh->cell[i].zc);
		    printf(" Node n1 (x,y,z)=(%lf, %lf, %lf) \n",mesh->node[mesh->cell[i].n1].x,mesh->node[mesh->cell[i].n1].y,mesh->node[mesh->cell[i].n1].z);
		    printf(" Node n2 (x,y,z)=(%lf, %lf, %lf) \n",mesh->node[mesh->cell[i].n2].x,mesh->node[mesh->cell[i].n2].y,mesh->node[mesh->cell[i].n2].z);
		    printf(" Node n3 (x,y,z)=(%lf, %lf, %lf) \n",mesh->node[mesh->cell[i].n3].x,mesh->node[mesh->cell[i].n3].y,mesh->node[mesh->cell[i].n3].z);
		    printf(" Node n4 (x,y,z)=(%lf, %lf, %lf) \n",mesh->node[mesh->cell[i].n4].x,mesh->node[mesh->cell[i].n4].y,mesh->node[mesh->cell[i].n4].z);
                printf(" Node n5 (x,y,z)=(%lf, %lf, %lf) \n",mesh->node[mesh->cell[i].n5].x,mesh->node[mesh->cell[i].n5].y,mesh->node[mesh->cell[i].n5].z);
		    printf(" Node n6 (x,y,z)=(%lf, %lf, %lf) \n",mesh->node[mesh->cell[i].n6].x,mesh->node[mesh->cell[i].n6].y,mesh->node[mesh->cell[i].n6].z);
		    printf(" Node n7 (x,y,z)=(%lf, %lf, %lf) \n",mesh->node[mesh->cell[i].n7].x,mesh->node[mesh->cell[i].n7].y,mesh->node[mesh->cell[i].n7].z);
		    printf(" Node n8 (x,y,z)=(%lf, %lf, %lf) \n",mesh->node[mesh->cell[i].n8].x,mesh->node[mesh->cell[i].n8].y,mesh->node[mesh->cell[i].n8].z);
		    printf(" \n");
                for(p=0;p<9;p++){
                printf(" stencil position X");
                printf(" %d ID: %d \n",p,mesh->cell[i].stX[p]);
                }
                printf(" \n");
                for(p=0;p<9;p++){
                printf(" stencil position Y");
                printf(" %d ID: %d \n",p,mesh->cell[i].stY[p]);
                }
                printf(" \n");
                for(p=0;p<9;p++){
                printf(" stencil position Z");
                printf(" %d ID: %d \n",p,mesh->cell[i].stZ[p]);
                }
                printf(" \n");
	      
	      }


	for(i=0;i<mesh->nwalls;i++){
		printf("pared %d tiene cellR: %d y cellL: %d\n",i,mesh->wall[i].cellR->id,mesh->wall[i].cellL->id);
	}
      
      getchar();
      
	#endif
	
	
	#if WRITE_TKE == 1
	file_tke=fopen("output-files/tke.out","w");
	#endif
	
	
	mass_calculation(mesh,sim);
	energy_calculation(mesh,sim);
	mass0=mesh->mass;
	energy0=mesh->energy;
	
	      
	////////////////////////////////////////////////////
	////////////// C A L C U L A T I O N ///////////////
	////////////////////////////////////////////////////
      
	tf=sim->tf;
      sim->t=0.0;
	t=0.0;
	nIt=0;
	
	#if ST!=0
      equilibrium_reconstruction(mesh,sim);
	#endif
      
	while(t<tf){
		
		cell=mesh->cell; 
		for(k=1;k<=sim->rk_steps;k++){
			if(k==1){
				compute_fluxes(mesh,sim);
				#if ST!=0
				compute_source(mesh);
				#endif
				#if LINEAR_TRANSPORT == 0
				update_dt(mesh,sim);
				#endif
				if(sim->order==1){
					update_cell(mesh,sim);
                              #if ALLOW_SOLIDS
                              update_ghost_cells(sim,mesh,solids);
                              #endif
				}else{
					update_cellK1(mesh,sim);
                              #if ALLOW_SOLIDS
                              update_ghost_cells(sim,mesh,solids);
                              #endif                               
				}
                        
                        /*for(n=0;n<mesh->zcells;n++){	
                              m = 1 + 0*mesh->xcells + n*mesh->xcells*mesh->ycells;
                              //cell=&(mesh->cell[m]);
                              cell[m].pres=(_gamma_-1.0)*(cell[m].U[4]-0.5*cell[m].U[0]*(cell[m].U[1]*cell[m].U[1]+cell[m].U[2]*cell[m].U[2])/(cell[m].U[0]*cell[m].U[0]));
                              cell[m].u_int=cell[m].pres/((_gamma_-1.0)*cell[m].U[0]); 
                              //printf("%lf %lf %lf %lf  \n",cell[m].zc,cell[m].w5->UR[4]*(_gamma_-1.0),cell[m].w6->UL[4]*(_gamma_-1.0),cell[m].w5->fR_star[3]-cell[m].w6->fL_star[3]+cell[m].S[3]*mesh->dz);
                              //printf("%lf %lf %lf %lf  \n",cell[m].zc,cell[m].w5->UR[4],cell[m].w6->UL[4],cell[m].w5->fR_star[3]-cell[m].w6->fL_star[3]+cell[m].S[3]*mesh->dz);
                              //printf("%lf %lf %lf  \n",cell[m].zc,cell[m].w5->UR[0],cell[m].w6->UL[0]);
                              //printf("%lf %14.14e   \n",cell[m].zc,cell[m].S_corr[3]);
					printf("%lf %lf %lf %lf %14.14e %14.14e %14.14e \n",cell[m].zc,cell[m].S[3],  cell[m].w5->fR_star[3], cell[m].w6->fL_star[3],    cell[m].w5->fR_star[3]-cell[m].w6->fL_star[3]+cell[m].S[3]*mesh->dz,      cell[m].U[3],  cell[m].U_aux[3]);
				
			     }
			     printf("\n");*/
                       //getchar();
			     
			     /*
			     for(n=0;n<mesh->zcells;n++){	
                              m = 1 + 0*mesh->xcells + n*mesh->xcells*mesh->ycells;
                              //cell=&(mesh->cell[m]);
                              //cell[m].pres=(_gamma_-1.0)*(cell[m].U[4]-0.5*cell[m].U[0]*(cell[m].U[1]*cell[m].U[1]+cell[m].U[2]*cell[m].U[2])/(cell[m].U[0]*cell[m].U[0]));
                              //cell[m].u_int=cell[m].pres/((_gamma_-1.0)*cell[m].U[0]); 
                              //printf("%lf %lf %lf %lf  \n",cell[m].zc,cell[m].w5->UR[4]*(_gamma_-1.0),cell[m].w6->UL[4]*(_gamma_-1.0),cell[m].w5->fR_star[3]-cell[m].w6->fL_star[3]+cell[m].S[3]*mesh->dz);
                              //printf("%lf %lf %lf %lf  \n",cell[m].zc,cell[m].w5->UR[4],cell[m].w6->UL[4],cell[m].w5->fR_star[3]-cell[m].w6->fL_star[3]+cell[m].S[3]*mesh->dz);
                              //printf("%lf %lf %lf  \n",cell[m].zc,cell[m].w5->UR[0],cell[m].w6->UL[0]);
                              //printf("%lf %14.14e   \n",cell[m].zc,cell[m].S_corr[3]);
					printf("%lf %14.14e %14.14e %14.14e %14.14e \n",cell[m].zc, cell[m].S[0],  cell[m].w5->fR_star[0], cell[m].w6->fL_star[0],    cell[m].w5->fR_star[0]-cell[m].w6->fL_star[0]+cell[m].S[0]*mesh->dz);
					printf("%lf %14.14e %14.14e %14.14e %14.14e \n",cell[m].zc, cell[m].S[1],  cell[m].w5->fR_star[1], cell[m].w6->fL_star[1],    cell[m].w5->fR_star[1]-cell[m].w6->fL_star[1]+cell[m].S[1]*mesh->dz);
					printf("%lf %14.14e %14.14e %14.14e %14.14e \n",cell[m].zc, cell[m].S[2],  cell[m].w5->fR_star[2], cell[m].w6->fL_star[2],    cell[m].w5->fR_star[2]-cell[m].w6->fL_star[2]+cell[m].S[2]*mesh->dz);
					printf("%lf %14.14e %14.14e %14.14e %14.14e \n",cell[m].zc, cell[m].S[3],  cell[m].w5->fR_star[3], cell[m].w6->fL_star[3],    cell[m].w5->fR_star[3]-cell[m].w6->fL_star[3]+cell[m].S[3]*mesh->dz);
					printf("%lf %14.14e %14.14e %14.14e %14.14e \n",cell[m].zc, cell[m].S[4],  cell[m].w5->fR_star[4], cell[m].w6->fL_star[4],    cell[m].w5->fR_star[4]-cell[m].w6->fL_star[4]+cell[m].S[4]*mesh->dz);
			     }
			     printf("\n");
                       getchar(); */


			}else if(k==2){
				compute_fluxes(mesh,sim);
				#if ST!=0
				compute_source(mesh);
				#endif
				update_cellK2(mesh,sim);
                        #if ALLOW_SOLIDS
                        update_ghost_cells(sim,mesh,solids);
                        #endif   
				/*for(n=0;n<mesh->zcells;n++){	
                              m = 1 + 0*mesh->xcells + n*mesh->xcells*mesh->ycells;
                              //cell=&(mesh->cell[m]);
                              cell[m].pres=(_gamma_-1.0)*(cell[m].U[4]-0.5*cell[m].U[0]*(cell[m].U[1]*cell[m].U[1]+cell[m].U[2]*cell[m].U[2])/(cell[m].U[0]*cell[m].U[0]));
                              cell[m].u_int=cell[m].pres/((_gamma_-1.0)*cell[m].U[0]); 
                              printf("%lf %lf %lf %lf %lf %lf %14.14e \n",cell[m].zc,cell[m].S[3],cell[m].w5->fR_star[3],cell[m].w6->fL_star[3],cell[m].w5->UR[4]*(_gamma_-1.0),cell[m].w6->UL[4]*(_gamma_-1.0),cell[m].w5->fR_star[3]-cell[m].w6->fL_star[3]+cell[m].S[3]*mesh->dz);
                        }
                        getchar();*/				
			}else{ //k=3
				compute_fluxes(mesh,sim);
				#if ST!=0
				compute_source(mesh);
				#endif
				update_cellK3(mesh,sim);
                        #if ALLOW_SOLIDS
                        update_ghost_cells(sim,mesh,solids);
                        #endif     
			}
		}
            
            
            #if LINEAR_TRANSPORT == 1	// this is an old feature
		    for(i=0;i<mesh->ncells;i++){
			  cell->U[0]=1.0;
			  cell++;
		    }
            #endif
		
		if(mesh->cell_bc_flag!=1){
			update_cell_boundaries(mesh);
		}


		////////////////////////////////////////////////////
		///////////// P O S T - P R O C E S S //////////////
		////////////////////////////////////////////////////

            timeac=timeac+sim->dt;
		if(timeac>sim->tVolc){	
			#if WRITE_VTK 
			sprintf(vtkfile,"output-files/state%03d.vtk",nIt+1);
			write_vtk(mesh,vtkfile);
			#endif
			#if WRITE_LIST 
			sprintf(listfile,"output-files/list%03d.out",nIt+1);
			write_list(mesh,listfile);
			#endif
			
                  printf("\n");
                  printf(" T= %lf, dt= %lf\n",t+sim->dt,sim->dt);
                  mass_calculation(mesh,sim);
			energy_calculation(mesh,sim);
                  printf(" Total mass: %14.14e\n",mesh->mass);
			printf(" Total energy: %14.14e\n",mesh->energy);
			printf(" Mass error: (M-M0)/M0 = %14.14e\n",(mesh->mass-mass0)/mass0);
			printf(" Energy error: (E-E0)/E0 = %14.14e\n",(mesh->energy-energy0)/energy0);
			printf("\n");
			nIt++;		//Number of iteration for the next time step
			timeac=0.0;
		}

		#if WRITE_TKE 
            timeac2=timeac2+sim->dt;
		if(timeac2>tTke){                
            tke_calculation(mesh,sim);
            fprintf(file_tke,"%14.14e %14.14e\n",t+sim->dt,mesh->tke);              
			timeac2=0.0;
		}
		#endif  
		
		
		t+=sim->dt;	//Time for the next time step
		sim->t=t;
            
	}
      
	
      printf(" \n");
      printf(" Final time is T= %14.14e \n \n",sim->t);
      
      if(timeac>TOL14){
      sprintf(vtkfile,"output-files/state%03d.vtk",nIt+1);
      write_vtk(mesh,vtkfile);
	  sprintf(listfile,"output-files/list%03d.out",nIt+1);
	  write_list(mesh,listfile);
      }
	  
      #if WRITE_TKE 
      fclose(file_tke);
      #endif  
	
	
	
	/*
	aux1=0.0;
	aux2=0.0;
      cell=mesh->cell; 
	for(n=5;n<mesh->zcells-5;n++){	
		m = 1 + 0*mesh->xcells + n*mesh->xcells*mesh->ycells;
		aux1=aux1+ABS(cell[m].S_corr[3]);
		aux2=aux2+1.0;
		//printf("%lf  %14.14e %14.14e\n",cell[m].zc,cell[m].S[3],cell[m].S_corr[3]);
	}
	printf("ST correction term is %14.14e \n",aux1/aux2);
	*/
      
/*      
      int m, l;
      FILE *f;
      char filename[1024];      
#if EULER      
      f=fopen("output-files/section-sod_schock_tube.out","w");
//		fprintf(f,"Rho, u, e, pres\n");

      cell=mesh->cell; 
      for(m=0;m<mesh->xcells;m++){	
            cell[m].pres=(_gamma_-1.0)*(cell[m].U[4]-0.5*cell[m].U[0]*(cell[m].U[1]*cell[m].U[1]+cell[m].U[2]*cell[m].U[2])/(cell[m].U[0]*cell[m].U[0]));
            cell[m].u_int=cell[m].pres/((_gamma_-1.0)*cell[m].U[0]); 
            fprintf(f,"%lf %lf %lf %lf %lf %lf\n",cell[m].xc,cell[m].U[0],cell[m].U[1]/cell[m].U[0],cell[m].u_int,cell[m].U[3],cell[m].pres);

      }
      fclose(f);
      
#endif 
*/ 



	printf("%s Under development. Please, be patient. Thanks!\n",END);



	return 1;

}

