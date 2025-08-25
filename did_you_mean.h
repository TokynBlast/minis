// did_you_mean.h
#pragma once
#include <string>
#include <vector>
#include <limits>
#include <algorithm>

inline int levenshtein(const std::string& a, const std::string& b){
    size_t n=a.size(), m=b.size();
    std::vector<int> dp(m+1);
    for (size_t j=0;j<=m;++j) dp[j]=(int)j;
    for (size_t i=1;i<=n;++i){
        int prev = dp[0]; dp[0]=(int)i;
        for (size_t j=1;j<=m;++j){
            int cur = dp[j];
            int cost = (a[i-1]==b[j-1]) ? 0 : 1;
            dp[j] = std::min({ dp[j] + 1, dp[j-1] + 1, prev + cost });
            prev = cur;
        }
    }
    return dp[m];
}

inline std::vector<std::string> best_suggestions(
    const std::string& needle, const std::vector<std::string>& dict, int max_suggestions=3)
{
    std::vector<std::pair<int,std::string>> scored;
    scored.reserve(dict.size());
    for (auto& w : dict) {
        int d = levenshtein(needle, w);
        scored.emplace_back(d, w);
    }
    std::sort(scored.begin(), scored.end(), [](auto& L, auto& R){ return L.first < R.first; });
    std::vector<std::string> out;
    for (int i=0;i<(int)scored.size() && i<max_suggestions; ++i) out.push_back(scored[i].second);
    return out;
}
