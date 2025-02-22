/* Copyright 2019 David Grote
 *
 * This file is part of WarpX.
 *
 * License: BSD-3-Clause-LBNL
 */
#ifndef WARPX_PSATD_ALGORITHM_GALILEAN_RZ_H_
#define WARPX_PSATD_ALGORITHM_GALILEAN_RZ_H_

#include "SpectralBaseAlgorithmRZ.H"

/* \brief Class that updates the field in spectral space
 * and stores the coefficients of the corresponding update equation.
 */
class PsatdAlgorithmGalileanRZ : public SpectralBaseAlgorithmRZ
{

    public:
        PsatdAlgorithmGalileanRZ (SpectralKSpaceRZ const & spectral_kspace,
                                  amrex::DistributionMapping const & dm,
                                  const SpectralFieldIndex& spectral_index,
                                  int const n_rz_azimuthal_modes, int const norder_z,
                                  bool const nodal,
                                  const amrex::Vector<amrex::Real>& v_galilean,
                                  amrex::Real const dt_step,
                                  bool const update_with_rho);
        // Redefine functions from base class
        virtual void pushSpectralFields (SpectralFieldDataRZ & f) override final;

        void InitializeSpectralCoefficients (SpectralFieldDataRZ const & f);

        /**
         * \brief Virtual function for current correction in Fourier space
         * This function overrides the virtual function \c CurrentCorrection in the
         * base class \c SpectralBaseAlgorithmRZ (qualifier \c override) and cannot be
         * overridden by further derived classes (qualifier \c final).
         *
         * \param[in,out] field_data all fields in Fourier space
         */
        virtual void CurrentCorrection (SpectralFieldDataRZ& field_data) override final;

        /**
         * \brief Virtual function for Vay current deposition in Fourier space
         * This function overrides the virtual function \c VayDeposition in the
         * base class \c SpectralBaseAlgorithmRZ and cannot be overridden by further
         * derived classes.
         *
         * \param[in,out] field_data All fields in Fourier space
         */
        virtual void VayDeposition (SpectralFieldDataRZ& field_data) override final;

    private:

        SpectralFieldIndex m_spectral_index;

        bool coefficients_initialized;
        // Note that dt and v_galilean are saved to use in InitializeSpectralCoefficients
        amrex::Real const m_dt;
        amrex::Vector<amrex::Real> m_v_galilean;
        bool m_update_with_rho;

        SpectralRealCoefficients C_coef, S_ck_coef;
        SpectralComplexCoefficients Theta2_coef, T_rho_coef, X1_coef, X2_coef, X3_coef, X4_coef;

};

#endif // WARPX_PSATD_ALGORITHM_GALILEAN_RZ_H_
