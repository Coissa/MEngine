#pragma once

namespace GE::Math
{
    enum class MatrixStorage { RowMajor };
    enum class VectorConvention { RowVector };
    enum class CoordinateHandedness { LeftHanded };
    enum class ClipSpaceDepth { ZeroToOne };
    enum class ShaderMatrixUpload { NoTranspose };

    inline constexpr MatrixStorage MatrixStorageConvention = MatrixStorage::RowMajor;
    inline constexpr VectorConvention VectorMultiplicationConvention = VectorConvention::RowVector;
    inline constexpr CoordinateHandedness HandednessConvention = CoordinateHandedness::LeftHanded;
    inline constexpr ClipSpaceDepth ClipSpaceDepthConvention = ClipSpaceDepth::ZeroToOne;
    inline constexpr ShaderMatrixUpload ShaderMatrixUploadConvention = ShaderMatrixUpload::NoTranspose;
}
