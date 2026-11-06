#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace GE::Core
{
    // Index-based disjoint-set (union-find) with path compression and union by size.
    class DisjointSet final
    {
    public:
        using Index = std::size_t;
        static constexpr Index InvalidIndex = std::numeric_limits<Index>::max();

        DisjointSet() = default;

        explicit DisjointSet(Index size) { Init(size); }

        void Init(Index size)
        {
            m_parents.resize(size);
            std::iota(m_parents.begin(), m_parents.end(), Index{0});
            m_sizes.assign(size, Index{1});
            m_set_count = size;
        }

        void Reset() noexcept
        {
            m_parents.clear();
            m_sizes.clear();
            m_set_count = 0;
        }

        void AddDefaulted(Index count)
        {
            if (count > m_parents.max_size() - m_parents.size())
            {
                throw std::length_error("DisjointSet size exceeds the container limit");
            }

            const Index old_size = m_parents.size();
            m_parents.resize(old_size + count);
            std::iota(m_parents.begin() + static_cast<std::ptrdiff_t>(old_size), m_parents.end(), old_size);
            m_sizes.resize(old_size + count, Index{1});
            m_set_count += count;
        }

        [[nodiscard]] Index Num() const noexcept { return m_parents.size(); }

        [[nodiscard]] Index NumSets() const noexcept { return m_set_count; }

        [[nodiscard]] bool IsEmpty() const noexcept { return m_parents.empty(); }

        [[nodiscard]] bool IsValidIndex(Index index) const noexcept { return index < m_parents.size(); }

        // Finds a representative and compresses the traversed path.
        [[nodiscard]] Index Find(Index index)
        {
            ValidateIndex(index);

            Index root = index;
            while (m_parents[root] != root)
            {
                root = m_parents[root];
            }

            while (m_parents[index] != index)
            {
                const Index parent = m_parents[index];
                m_parents[index] = root;
                index = parent;
            }
            return root;
        }

        // Const lookup intentionally does not mutate the parent forest.
        [[nodiscard]] Index Find(Index index) const
        {
            ValidateIndex(index);
            while (m_parents[index] != index)
            {
                index = m_parents[index];
            }
            return index;
        }

        [[nodiscard]] Index operator[](Index index) const { return Find(index); }

        // Returns true only when two previously separate sets were merged.
        [[nodiscard]] bool Union(Index left, Index right)
        {
            Index left_root = Find(left);
            Index right_root = Find(right);
            if (left_root == right_root)
            {
                return false;
            }

            if (m_sizes[left_root] < m_sizes[right_root])
            {
                std::swap(left_root, right_root);
            }

            m_parents[right_root] = left_root;
            m_sizes[left_root] += m_sizes[right_root];
            m_sizes[right_root] = 0;
            --m_set_count;
            return true;
        }

        // API-compatible convenience for sequential construction. Union by size already
        // keeps the forest shallow, so the same implementation is used here.
        [[nodiscard]] bool UnionSequential(Index left, Index right) { return Union(left, right); }

        [[nodiscard]] bool IsConnected(Index left, Index right) const { return Find(left) == Find(right); }

        [[nodiscard]] Index SetSize(Index index) const { return m_sizes[Find(index)]; }

        [[nodiscard]] std::vector<Index> MembersOf(Index index) const
        {
            const Index root = Find(index);
            std::vector<Index> members;
            members.reserve(m_sizes[root]);
            for (Index candidate = 0; candidate < Num(); ++candidate)
            {
                if (Find(candidate) == root)
                {
                    members.push_back(candidate);
                }
            }
            return members;
        }

        [[nodiscard]] std::vector<std::vector<Index>> GetSets() const
        {
            std::vector<std::vector<Index>> sets(Num());
            for (Index index = 0; index < Num(); ++index)
            {
                sets[Find(index)].push_back(index);
            }

            std::vector<std::vector<Index>> result;
            result.reserve(NumSets());
            for (auto& set : sets)
            {
                if (!set.empty())
                {
                    result.push_back(std::move(set));
                }
            }
            return result;
        }

        // Visits every unordered pair. A bool-like return value decides whether the pair
        // should be merged. A void callback may call Union itself. The return value is the
        // number of sets merged while the callback was running.
        template <typename Function>
            requires std::invocable<Function&, Index, Index>
        Index ForAll(Function&& function)
        {
            const Index previous_set_count = NumSets();
            for (Index left = 0; left < Num(); ++left)
            {
                for (Index right = left + 1; right < Num(); ++right)
                {
                    if constexpr (std::is_void_v<std::invoke_result_t<Function&, Index, Index>>)
                    {
                        std::invoke(function, left, right);
                    }
                    else
                    {
                        static_assert(std::convertible_to<std::invoke_result_t<Function&, Index, Index>, bool>,
                                      "A non-void DisjointSet::ForAll callback must return a bool-like value");
                        if (static_cast<bool>(std::invoke(function, left, right)))
                        {
                            static_cast<void>(Union(left, right));
                        }
                    }
                }
            }
            return previous_set_count - NumSets();
        }

        // Visits every element once. This form is useful when the callback discovers and
        // unions an arbitrary number of neighbours for each element.
        template <typename Function>
            requires(!std::invocable<Function&, Index, Index> && std::invocable<Function&, Index>)
        Index ForAll(Function&& function)
        {
            const Index previous_set_count = NumSets();
            const Index element_count = Num();
            for (Index index = 0; index < element_count; ++index)
            {
                std::invoke(function, index);
            }
            return previous_set_count - NumSets();
        }

    private:
        void ValidateIndex(Index index) const
        {
            if (!IsValidIndex(index))
            {
                throw std::out_of_range("DisjointSet index is out of range");
            }
        }

        std::vector<Index> m_parents;
        std::vector<Index> m_sizes;
        Index m_set_count = 0;
    };

    // Familiar UE-style spelling for code that prefers the F-prefix.
    using FDisjointSet = DisjointSet;
} // namespace GE::Core
