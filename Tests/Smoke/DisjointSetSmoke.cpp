#include <Core/Containers/DisjointSet.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    [[nodiscard]] int Fail(int exit_code, const char* message)
    {
        std::cerr << "[DisjointSetSmoke] Failure: " << message << '\n';
        return exit_code;
    }
} // namespace

int main()
{
    try
    {
        using GE::Core::DisjointSet;
        using Index = DisjointSet::Index;

        const std::array values{1, 3, 8, 10, 15, 17};
        DisjointSet sets(values.size());

        const Index predicate_merges =
            sets.ForAll([&values](Index left, Index right) { return std::abs(values[left] - values[right]) <= 2; });

        if (predicate_merges != 3 || sets.NumSets() != 3)
        {
            return Fail(1, "the binary predicate did not create three components");
        }
        if (!sets.IsConnected(0, 1) || sets.IsConnected(1, 2))
        {
            return Fail(2, "connectivity query returned an incorrect result");
        }
        if (sets.SetSize(0) != 2 || sets.MembersOf(2) != std::vector<Index>({2, 3}))
        {
            return Fail(3, "set size or member query returned an incorrect result");
        }

        const Index callback_merges = sets.ForAll(
            [&sets](Index left, Index right)
            {
                if (left == 0 && right == 2)
                {
                    static_cast<void>(sets.Union(left, right));
                }
            });
        if (callback_merges != 1 || sets.NumSets() != 2 || sets.SetSize(3) != 4)
        {
            return Fail(4, "the void callback could not merge sets explicitly");
        }

        const Index unary_merges = sets.ForAll(
            [&sets](Index index)
            {
                if (index > 0)
                {
                    static_cast<void>(sets.Union(0, index));
                }
            });
        if (unary_merges != 1 || sets.NumSets() != 1 || sets[5] != sets.Find(0))
        {
            return Fail(5, "the unary callback or representative query failed");
        }

        const auto groups = sets.GetSets();
        if (groups.size() != 1 || groups.front().size() != values.size())
        {
            return Fail(6, "GetSets did not return the complete partition");
        }

        sets.AddDefaulted(2);
        if (sets.Num() != 8 || sets.NumSets() != 3 || sets.SetSize(7) != 1)
        {
            return Fail(7, "AddDefaulted did not append singleton sets");
        }
        if (!sets.UnionSequential(6, 7) || sets.NumSets() != 2)
        {
            return Fail(8, "UnionSequential did not merge appended sets");
        }

        bool invalid_index_rejected = false;
        try
        {
            static_cast<void>(sets.Find(sets.Num()));
        }
        catch (const std::out_of_range&)
        {
            invalid_index_rejected = true;
        }
        if (!invalid_index_rejected)
        {
            return Fail(9, "an invalid index was not rejected");
        }

        sets.Reset();
        if (!sets.IsEmpty() || sets.Num() != 0 || sets.NumSets() != 0)
        {
            return Fail(10, "Reset did not clear the data structure");
        }

        std::cout << "[DisjointSetSmoke] Passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[DisjointSetSmoke] Unhandled exception: " << exception.what() << '\n';
        return 100;
    }
    catch (...)
    {
        std::cerr << "[DisjointSetSmoke] Unknown exception\n";
        return 101;
    }
}
