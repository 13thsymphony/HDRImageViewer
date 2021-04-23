#pragma once
#include <vector>
#include <cassert>
struct Matrix
{
    Matrix(size_t x, size_t y) : X(x), Y(y)
    {
        M.resize(x * y);
    }

    double& Index(size_t i_x, size_t i_y)
    {
        return M[i_y * X + i_x];
    }

    Matrix operator*(const Matrix& rhs)
    {
        assert(X == rhs.Y);
        Matrix ret(rhs.X, Y);

        size_t x, y;
        for (size_t i = 0; i < ret.M.size(); i++)
        {
            x = i % ret.X;
            y = i / ret.X;

            ret.M[i] = 0;

            for (size_t r = 0; r < X; r++)
            {
                ret.M[i] += M[X * y + r] * rhs.M[rhs.X * r + x];
            }
        }

        return ret;
    }

    Matrix SubMatrix(size_t x_start, size_t y_start, size_t width, size_t height)
    {
        assert((x_start + width) <= X && (y_start + height) <= Y);

        Matrix ret(width, height);
        size_t index = 0;

        for (size_t y = y_start; y < y_start + height; y++)
        {
            for (size_t x = x_start; x < x_start + width; x++)
            {
                ret.M[index++] = M[y * X + x % X];
            }
        }

        return ret;
    }

    double Determinant()
    {
        assert(X == Y);

        // I'm lazy and only worrying about the 3x3 case right now.
        assert(Y == 3);

        double ret = 0;

        for (size_t x = 0; x < X; x++)
        {
            ret += (Index(0, x) *
                (Index(1, (x + 1) % 3) * Index(2, (x + 2) % 3) -
                    Index(1, (x + 2) % 3) * Index(2, (x + 1) % 3)));
        }

        return ret;
    }

    Matrix Invert()
    {
        // Basically all color matrices are 3x3 - I'm only worrying about that here
        assert(X == Y && X == 3);

        Matrix ret(3, 3);
        double det = Determinant();

        assert(det > 0);

        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                ret.Index(i, j) = ((Index((j + 1) % 3, (i + 1) % 3) * Index((j + 2) % 3, (i + 2) % 3)) - (Index((j + 1) % 3, (i + 2) % 3) * Index((j + 2) % 3, (i + 1) % 3))) / det;
            }
        }

        return ret;
    }

    const size_t X, Y;
    std::vector<double> M;
};