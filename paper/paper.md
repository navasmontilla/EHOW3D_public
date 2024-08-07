---
title: 'EHOW3D: An academic High Order Finite Volume solver for hyperbolic problems'
tags:
  - fluid mechanics
  - compressible flow
  - atmospheric flow
  - finite volume
  - high order
authors:
  - name: Adrian Navas-Montilla
    orcid: 0000-0002-3465-6898
    equal-contrib: true
    affiliation: 1 # (Multiple affiliations must be quoted)
  - name: Isabel Echeverribar
    orcid: 0000-0000-0000-0000
    equal-contrib: true
    affiliation: 1 # (Multiple affiliations must be quoted)
affiliations:
 - name: Fluid Dynamics Technologies, i3A-University of Zaragoza, Spain
   index: 1
 - name: Institution Name, Country
   index: 2
date: 4 August 2024
bibliography: paper.bib

---

# Summary

In the last decade, very high order numerical methods have become very popular within the Computational Fluid Dynamics (CFD) and Numerical Weather Prediction (NWP) communities. Traditional low order --i.e. equal or lower than 2-- methods have been widely adopted by industry and academia to simulate fluid flows, but they show important limitations as they feature high diffusion and dispersion errors which may lead to non-physical solutions [@FERRER2023108700]. On the other hand, high order schemes are able to provide accurate solutions, with small diffusion and dispersion errors, and feature a high computational efficiency. They are well suited for multi-scale problems, e.g. turbulent flows, flows involving sharp gradients such as shocks, acoustic phenomena, etc. These cutting-edge approaches have gained attention due to their notable performance when running on modern High-Performance Computing (HPC) architectures.

The increased accuracy and computational efficiency have favored the development and utilization of Large Eddy Simulation (LES) models. They are designed to capture the largest turbulent structures by numerically solving a filtered version of the Navier–Stokes equations, while modeling the smaller eddies explicitly using a dissipative sub-grid model. Implicit Large Eddy Simulation (iLES) methods offer an alternative to traditional LES methods. In iLES methods, numerical diffusion plays the role of the explicit dissipative sub-grid model. They are a favorable choice when seeking simplicity and computational efficiency, as they do not require the implementation of a sub-grid model. 

`EHOW3D` is a hydrodynamic iLES solver for compressible flows, which solves the Euler equations of gas dynamics by means of very high order essentially non-oscillatory finite volume schemes in cartesian meshes. The Weighted Essentially Non-Oscillatory (WENO) and Targeted Essentially Non-Oscillatory (TENO) spatial reconstructions, in combination with Runge-Kutta integrators, are excellent candidates for this purpose and are used in `EHOW3D`. The resulting hydrodynamic solver can be applied to the computation of compressible turbulence, shock waves and shock-induced turbulence. Additionally, it can also be applied to low Mach number flows, such as atmospheric flows and other wave propagation phenomena.  

# Statement of need

`Gala` is an Astropy-affiliated Python package for galactic dynamics. Python
enables wrapping low-level languages (e.g., C) for speed without losing
flexibility or ease-of-use in the user-interface. The API for `Gala` was
designed to provide a class-based and user-friendly interface to fast (C or
Cython-optimized) implementations of common operations such as gravitational
potential and force evaluation, orbit integration, dynamical transformations,
and chaos indicators for nonlinear dynamics. `Gala` also relies heavily on and
interfaces well with the implementations of physical units and astronomical
coordinate systems in the `Astropy` package [@astropy] (`astropy.units` and
`astropy.coordinates`).

`Gala` was designed to be used by both astronomical researchers and by
students in courses on gravitational dynamics or astronomy. It has already been
used in a number of scientific publications [@Pearson:2017] and has also been
used in graduate courses on Galactic dynamics to, e.g., provide interactive
visualizations of textbook material [@Binney:2008]. The combination of speed,
design, and support for Astropy functionality in `Gala` will enable exciting
scientific explorations of forthcoming data releases from the *Gaia* mission
[@gaia] by students and experts alike.

# Mathematics

Single dollars ($) are required for inline mathematics e.g. $f(x) = e^{\pi/x}$

This software allows to compute the solution of the scalar linear advection equation
$$\frac{\partial u}{\partial t} + \nabla \cdot ( \mathbf{v} u) = 0$$
where $u$ is the transported quantity and $\mathbf{v}$ is the advection velocity.  When setting considering $\mathbf{v}=1/2(u,u,u)^T$, we have the Burgers' equation. We can also compute the compressible Euler equations with gravitational source term, given by
\begin{align}
\frac{\partial \rho}{\partial t} + \nabla \cdot (\rho \mathbf{v}) &= 0 \tag{Continuity} \\
\frac{\partial (\rho \mathbf{v})}{\partial t} + \nabla \cdot \left(\rho \mathbf{v} \otimes \mathbf{v} + p \mathbf{I}\right) &= \rho \mathbf{g} \tag{Momentum} \\
\frac{\partial E}{\partial t} + \nabla \cdot \left((E + p) \mathbf{v}\right) &= \rho \mathbf{v} \cdot \mathbf{g} \tag{Energy}
\end{align}
where $\rho$ is density, $\mathbf{v}$ is the velocity vector, $p$ is pressure and $\mathbf{g}=(0,0,g)^T$ is the gravitational acceleration vector. The energy is defined as  the sum of kinetic and internal energy $E=\rho(\frac{1}{2}\mathbf{v}+e)$.

You can also use plain \LaTeX for equations
\begin{equation}\label{eq:fourier}
\hat f(\omega) = \int_{-\infty}^{\infty} f(x) e^{i\omega x} dx
\end{equation}
and refer to \autoref{eq:fourier} from text.

# Citations

Citations to entries in paper.bib should be in
[rMarkdown](http://rmarkdown.rstudio.com/authoring_bibliographies_and_citations.html)
format.

If you want to cite a software repository URL (e.g. something on GitHub without a preferred
citation) then you can do it with the example BibTeX entry below for @fidgit.

For a quick reference, the following citation commands can be used:
- `@author:2001`  ->  "Author et al. (2001)"
- `[@author:2001]` -> "(Author et al., 2001)"
- `[@author1:2001; @author2:2001]` -> "(Author1 et al., 2001; Author2 et al., 2002)"

# Figures

Figures can be included like this:
![Caption for example figure.\label{fig:example}](figure.png)
and referenced from text using \autoref{fig:example}.

Figure sizes can be customized by adding an optional second parameter:
![Caption for example figure.](figure.png){ width=20% }

# Acknowledgements

We acknowledge contributions from Brigitta Sipocz, Syrtis Major, and Semyeong
Oh, and support from Kathryn Johnston during the genesis of this project.

# References