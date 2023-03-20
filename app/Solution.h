//
// Created by awen on 23-3-16.
//

#ifndef NGINX_SOLUTION_H
#define NGINX_SOLUTION_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <map>
#include <set>
#include <functional>
using namespace std;

struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};
class Solution {
public:
    int search(vector<int>&nums,int left,int right, int target){
        int start=left,end=right;
        while (start<=end){
            int mid = (start - end)/2 + end;
            if(nums[mid]<target){
                start = mid + 1;
            } else if(nums[mid]>target){
                end = mid - 1;
            } else{
                return mid;
            }
        }
        return -1;
    }
    string findLexSmallestString(string s, int a, int b) {
        int n = s.size();
        string ans;
        for (int i = 0; i < 10; ++i) {
            string s1 = s;
            for (int j = 1; j < n; j += 2) {
                s1[j] = (s1[i]-'0'+a)%10 + '0';
            }

        }
    }
    int count = 0;
    int findTargetSumWays(vector<int>& nums, int target) {
        int n =nums.size();
        int ans = 0;
        int cnt = 0;
        int dp[2001][21];
//        function<void(int)> dfs = [&](int i){
//            int tmp = ans;
//            if(i==n) {
//                if(cnt==target){
//                    ans++;
//                    return ;
//                }
//                return;
//            }
//            if(dp[cnt+1000][n-i]!=0){
//                ans += dp[cnt+1000][n-i];
//                return;
//            }
//            cnt -= nums[i];
//            dfs(i+1);
//            cnt += nums[i];
//
//
//            cnt += nums[i];
//            dfs(i+1);
//            cnt -= nums[i];
//
//            dp[cnt+1000][n-i] =ans-tmp;
//        };
        dfs(0, target, nums, 0);
        return ans;
    }


};



#endif //NGINX_SOLUTION_H
