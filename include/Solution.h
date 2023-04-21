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
#include <memory>
#include <cstring>
#include <stack>
using namespace std;

struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

  struct TreeNode {
      int val;
      TreeNode *left;
      TreeNode *right;
      TreeNode(int x) : val(x), left(NULL), right(NULL) {}
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

    bool checkInclusion(string s1, string s2) {
        int n_2 = s2.size();
        int n_1 = s1.size();
        map<int,int> map_1;
        map<int,int> map_2;
        for (int i = 0; i < n_1; ++i) {
            if(!map_1.count(s1[i])){
                map_1.insert({s1[i],0});
            }
            map_1[s1[i]] += 1;
            if(!map_2.count(s2[i])){
                map_2.insert({s2[i],0});
            }
            map_2[s2[i]] += 1;
        }
        for (int i = 0; i < n_2-n_1; ++i) {
            if(map_1==map_2){
                return true;
            }
        }
    }
    int maxEnvelopes(vector<vector<int>>& envelopes) {
        int n = envelopes.size();
        int ans = 0;
//        quick(envelopes,0,n-1);
        sort(envelopes.begin(),envelopes.end(),[](vector<int>& a, vector<int>& b) {
            return a[0] == b[0] ?
                   b[1] - a[1] : a[0] - b[0];
        });
        vector<int> dp(n+1,1);
        for (int i = 1; i < n+1; ++i) {
            for (int j = 1; j < i; ++j) {
                if(envelopes[i-1][1]>=envelopes[j-1][1]){
                    dp[i] = max(dp[i],dp[j]+1);
                }
            }
        }
        for (int i = 1; i < n+1; ++i) {
            ans = max(ans,dp[i]);
        }
        return ans;
    }

    void quick(vector<vector<int>>& envelopes,int left,int right){
        if(left>=right){
            return;
        }
        int l = left,r = right;
        int temp = left;
        while (left<right){
            while (envelopes[right][0]>envelopes[temp][0]&&left<right){
                right--;
            }
            if(left<right) {
                swap(envelopes[temp], envelopes[right]);
                temp = right;
                left++;
            }

            while (envelopes[left][0]<envelopes[temp][0]&&left<right){
                left++;
            }
            if(left<right) {
                swap(envelopes[temp], envelopes[left]);
                temp = left;
                right--;
            }
        }
        quick(envelopes,l,temp-1);
        quick(envelopes,temp+1,r);
    }

    vector<int> sortArray(vector<int>& nums) {
        quick1(nums,0,nums.size()-1);
        return nums;
    }

    void quick1(vector<int>& envelopes,int left,int right){
        if(left>=right){
            return;
        }
        int l = left,r = right;
        int temp = left;
        while (left<right){
            while (envelopes[right]>envelopes[temp]&&left<right){
                right--;
            }
            if(left<right) {
                swap(envelopes[temp], envelopes[right]);
                temp = right;
                left++;
            }

            while (envelopes[left]<envelopes[temp]&&left<right){
                left++;
            }
            if(left<right) {
                swap(envelopes[temp], envelopes[left]);
                temp = left;
                right--;
            }
        }
        quick1(envelopes,l,temp-1);
        quick1(envelopes,temp+1,r);
    }
};

int add(int a,int b);

void address();

#endif //NGINX_SOLUTION_H
