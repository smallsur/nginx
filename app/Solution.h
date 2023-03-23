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


    int findTargetSumWays(vector<int>& nums, int target) {
        int n =nums.size();
        int dp[2001][21]{};
        for (int i = 1; i < n+1; ++i) {
            for (int j = 1; j < n+1; ++j) {
                dp[nums[i]+1000][j]=1;
            }
        }
        for (int i = 1; i < n + 1; ++i) {
            int num = nums[i];
            for (int j = 0; j < 2001; ++j) {
                if(j-num>0){
                    dp[j][i] +=  dp[j-num][i-1];
                }
                if(j+num<2001){
                    dp[j][i] +=  dp[j+num][i-1];
                }
            }
        }
        return dp[target+1000][n];
    }

    int lengthOfLongestSubstring(string s) {
        int left = 0,right = 0;
        int n = s.size();
        map<int,int> m;
        int ans = -1;
        while (right<n){
            int num = s[right];
            if(!m.count(num)){
                m.insert({num,right});
            } else if(m[num]!=-1){
                left = m[num];
                m[num] = -1;
                left++;
            }else{
                left++;
            }
            right++;
            ans = max(ans, right-left);
        }
        return ans;
    }


};



#endif //NGINX_SOLUTION_H
