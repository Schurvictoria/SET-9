#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <cassert>
#include <numeric>
#include <functional>
#include <unordered_map>

class StringGenerator {
public:
    enum class Kind { Random, Reverse, AlmostSorted };

    StringGenerator(unsigned seed = std::random_device{}())
        : gen(seed),
          distLen(10, 200)
    {
        alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789"
            "!@#%:;^&*()-.";
        distChar = std::uniform_int_distribution<int>(0, (int)alphabet.size() - 1);

        maxSize = 3000;
        for (Kind kind : {Kind::Random, Kind::Reverse, Kind::AlmostSorted}) {
            auto bigSample = generateRawSample(maxSize);
            if (kind == Kind::Reverse) {
                std::sort(bigSample.begin(), bigSample.end(), std::greater<>());
            } else if (kind == Kind::AlmostSorted) {
                std::sort(bigSample.begin(), bigSample.end());
                for (size_t i = 0; i + 1 < bigSample.size(); i += 10)
                    std::swap(bigSample[i], bigSample[i + 1]);
            }
            samples[kind] = std::move(bigSample);
        }
    }

    std::vector<std::string> getSample(std::size_t size, Kind kind) {
        assert(size <= maxSize);
        return std::vector<std::string>(samples[kind].begin(), samples[kind].begin() + size);
    }

private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> distLen;
    std::uniform_int_distribution<int> distChar;
    std::string alphabet;
    std::size_t maxSize;

    std::unordered_map<Kind, std::vector<std::string>> samples;

    std::vector<std::string> generateRawSample(std::size_t size) {
        std::vector<std::string> res;
        res.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            int len = distLen(gen);
            std::string s(len, ' ');
            for (int j = 0; j < len; ++j) {
                s[j] = alphabet[distChar(gen)];
            }
            res.push_back(std::move(s));
        }
        return res;
    }
};

struct SortResult {
    std::chrono::milliseconds time;
    std::size_t comps;
};

class StringSortTester {
public:
    enum class Algo { StdQuick, StdMergeLCP, TernaryQuick, MsdRadix, MsdRadixPure };

    SortResult run(Algo algo, std::vector<std::string>& arr) {
        comps = 0;
        auto start = std::chrono::high_resolution_clock::now();

        switch (algo) {
        case Algo::StdQuick:
            std::sort(arr.begin(), arr.end(), [this](const std::string& a, const std::string& b) {
                ++comps;
                return a < b;
            });
            break;
        case Algo::StdMergeLCP:
            mergeSortLCP(arr, 0, arr.size());
            break;
        case Algo::TernaryQuick:
            ternaryQuickSort(arr, 0, arr.size() - 1);
            break;
        case Algo::MsdRadix:
            msdRadixSort(arr, 0, arr.size(), 0);
            break;
        case Algo::MsdRadixPure:
            msdRadixSortPure(arr, 0, arr.size(), 0);
            break;
        }

        auto end = std::chrono::high_resolution_clock::now();
        return { std::chrono::duration_cast<std::chrono::milliseconds>(end - start), comps };
    }

private:
    std::size_t comps = 0;

    int lcp(const std::string& a, const std::string& b) {
        int i = 0;
        while (i < (int)a.size() && i < (int)b.size() && a[i] == b[i]) ++i;
        return i;
    }

    void mergeSortLCP(std::vector<std::string>& arr, std::size_t left, std::size_t right) {
        if (right - left <= 1) return;
        std::size_t mid = (left + right) / 2;
        mergeSortLCP(arr, left, mid);
        mergeSortLCP(arr, mid, right);

        std::vector<std::string> temp;
        temp.reserve(right - left);

        size_t i = left, j = mid;

        while (i < mid && j < right) {
            ++comps;
            if (arr[i] <= arr[j])
                temp.push_back(arr[i++]);
            else
                temp.push_back(arr[j++]);
        }
        while (i < mid) temp.push_back(arr[i++]);
        while (j < right) temp.push_back(arr[j++]);

        std::copy(temp.begin(), temp.end(), arr.begin() + left);
    }

    void ternaryQuickSort(std::vector<std::string>& arr, int lo, int hi) {
        if (lo >= hi) return;
        int lt = lo, gt = hi;
        const std::string& pivot = arr[lo];
        int i = lo + 1;
        while (i <= gt) {
            ++comps;
            if (arr[i] < pivot) std::swap(arr[lt++], arr[i++]);
            else if (arr[i] > pivot) std::swap(arr[i], arr[gt--]);
            else ++i;
        }
        ternaryQuickSort(arr, lo, lt - 1);
        ternaryQuickSort(arr, gt + 1, hi);
    }

    static constexpr int R = 74;
    static constexpr int cutoff = 15;

    int charToIndex(char c) {
        static std::string alpha =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789"
            "!@#%:;^&*()-.";
        auto pos = alpha.find(c);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    int charAt(const std::string& s, std::size_t d) {
        return d < s.length() ? charToIndex(s[d]) : -1;
    }

    void msdRadixSort(std::vector<std::string>& arr, std::size_t left, std::size_t right, std::size_t d) {
        if (right <= left + 1) return;
        if (right - left <= cutoff) {
            ternaryQuickSortSuffix(arr, left, right - 1, d);
            return;
        }
        std::vector<std::size_t> count(R + 2, 0);
        for (std::size_t i = left; i < right; ++i) {
            ++count[charAt(arr[i], d) + 2];
            ++comps;
        }
        for (int r = 0; r < R + 1; ++r)
            count[r + 1] += count[r];
        std::vector<std::string> aux(right - left);
        for (std::size_t i = left; i < right; ++i)
            aux[count[charAt(arr[i], d) + 1]++] = std::move(arr[i]);
        for (std::size_t i = 0; i < aux.size(); ++i)
            arr[left + i] = std::move(aux[i]);
        for (int r = 0; r < R; ++r)
            msdRadixSort(arr, left + count[r], left + count[r + 1], d + 1);
    }

    void msdRadixSortPure(std::vector<std::string>& arr, std::size_t left, std::size_t right, std::size_t d) {
        if (right <= left + 1) return;
        std::vector<std::size_t> count(R + 2, 0);
        for (std::size_t i = left; i < right; ++i) {
            ++count[charAt(arr[i], d) + 2];
            ++comps;
        }
        for (int r = 0; r < R + 1; ++r)
            count[r + 1] += count[r];
        std::vector<std::string> aux(right - left);
        for (std::size_t i = left; i < right; ++i)
            aux[count[charAt(arr[i], d) + 1]++] = std::move(arr[i]);
        for (std::size_t i = 0; i < aux.size(); ++i)
            arr[left + i] = std::move(aux[i]);
        for (int r = 0; r < R; ++r)
            msdRadixSortPure(arr, left + count[r], left + count[r + 1], d + 1);
    }

    void ternaryQuickSortSuffix(std::vector<std::string>& arr, int lo, int hi, std::size_t d) {
        if (lo >= hi) return;
        int lt = lo, gt = hi;
        const std::string& pivot = arr[lo];
        int i = lo + 1;
        while (i <= gt) {
            ++comps;
            if (suffixLess(arr[i], pivot, d)) std::swap(arr[lt++], arr[i++]);
            else if (suffixLess(pivot, arr[i], d)) std::swap(arr[i], arr[gt--]);
            else ++i;
        }
        ternaryQuickSortSuffix(arr, lo, lt - 1, d);
        ternaryQuickSortSuffix(arr, gt + 1, hi, d);
    }

    bool suffixLess(const std::string& a, const std::string& b, std::size_t d) {
        size_t i = d;
        while (i < a.size() && i < b.size()) {
            ++comps;
            if (a[i] < b[i]) return true;
            if (a[i] > b[i]) return false;
            ++i;
        }
        ++comps;
        return a.size() < b.size();
    }
};

template<typename Func>
SortResult averageRun(Func f, int runs = 5) {
    std::vector<SortResult> results;
    for (int i = 0; i < runs; ++i) {
        results.push_back(f());
    }
    long long totalTime = 0;
    long long totalComps = 0;
    for (auto& r : results) {
        totalTime += r.time.count();
        totalComps += r.comps;
    }
    return SortResult{ std::chrono::milliseconds(totalTime / runs), static_cast<std::size_t>(totalComps / runs) };
}

int main() {
    StringGenerator gen(42);
    StringSortTester tester;

    const std::vector<StringGenerator::Kind> kinds = {
        StringGenerator::Kind::Random,
        StringGenerator::Kind::Reverse,
        StringGenerator::Kind::AlmostSorted
    };

    const std::vector<std::string> kindNames = {
        "Random", "Reverse sorted", "Almost sorted"
    };

    std::vector<StringSortTester::Algo> algos = {
        StringSortTester::Algo::StdQuick,
        StringSortTester::Algo::StdMergeLCP,
        StringSortTester::Algo::TernaryQuick,
        StringSortTester::Algo::MsdRadix,
        StringSortTester::Algo::MsdRadixPure
    };

    std::vector<std::string> algoNames = {
        "QuickSort",
        "MergeSort with LCP",
        "Ternary QuickSort",
        "MSD Radix Sort with cutoff",
        "MSD Radix Sort pure"
    };

    for (size_t n = 100; n <= 3000; n += 100) {
        for (size_t ki = 0; ki < kinds.size(); ++ki) {
            auto kind = kinds[ki];
            const auto& kindName = kindNames[ki];
            std::cout << kindName << " array size " << n << "\n";

            for (size_t ai = 0; ai < algos.size(); ++ai) {
                auto algo = algos[ai];
                auto algoName = algoNames[ai];

                auto sample = gen.getSample(n, kind);

                auto res = averageRun([&]() {
                    auto arrCopy = sample;
                    return tester.run(algo, arrCopy);
                }, 5);

                std::cout << algoName << "\tTime: " << res.time.count() << " ms\tChar comparisons: " << res.comps << "\n";
            }
            std::cout << "\n";
        }
    }
    return 0;
}
