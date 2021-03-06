//! Author: Martin K. Schröder <mkschreder.uk@gmail.com>

JUCI.app
.directive("juciErrors", function(){
	return {
		// accepted parameters for this tag
		scope: {
			ngModel: "="
		}, 
		templateUrl: "/widgets/juci-errors.html", 
		replace: true, 
		controller: "juciErrors"
	}; 
})
.controller("juciErrors", function($scope, $rootScope, $localStorage){
	$scope.$watch("ngModel", function(value){
		if(value) $scope.errors = $scope.ngModel; 
		else $scope.errors = $rootScope.errors; 
	}); 
}); 

JUCI.app
.directive("juciError", function(){
	return {
		// accepted parameters for this tag
		scope: {
			value: "="
		}, 
		template: '<div ng-show="value" class="alert-danger" style="margin-top: 10px; font-size: 0.8em; padding: 5px; border-radius: 5px">{{value}}</div>', 
		replace: true
	}; 
}); 
