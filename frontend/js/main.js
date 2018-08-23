function registerLedger(mobileText) {

    var publicKey = document.getElementById("public-key-input" + mobileText).value;
    var period = document.getElementById("period-input" + mobileText).value;
    var currentHashBlock = document.getElementById("current-block-hash-input" + mobileText).value;
    var password = document.getElementById("password-input" + mobileText).value;

    var datatest = JSON.stringify({publickey: publicKey , timeperiod: period, currenthash: currentHashBlock,
                                    password: password});
    console.log(datatest);

    $.ajax({
        url: 'http://localhost:3000/register',
        type: 'POST',
        contentType:'application/json',
        data: datatest,
        dataType:'json'
    });
}

function checkLedger(mobileText){

    $.ajax({
        url: 'http://localhost:3000/check',
        type: 'GET',
        contentType: 'application/json',
        success: function(data){
            var json = JSON.parse(data);
            if (json.privatekey) {
                document.getElementById("check-result" + mobileText).innerHTML = "Result: &nbsp;&nbsp;&nbsp;&nbsp;<span style=\"color:red; font-size:150%; font-weight: bold;\">Dead man! &#9760;</span>" +
                    "<br><span style=\"color:red; font-size:100%\">Password: " + json.privatekey + "</span>";
            }else {
                document.getElementById("check-result" + mobileText).innerHTML = "Result: &nbsp;&nbsp;&nbsp;&nbsp;<span style=\"color:green; font-size:150%; font-weight: bold;\">Still alive! &#10004;</span>";
            }
        },
        error: function(err){
            console.log(err);
        }
    });

}



$(document).ready(function() {
    var mobileText = "";
    if (document.documentElement.clientWidth <= 480) mobileText = "-mobile";

     document.getElementById("register-button" + mobileText).onclick = function(){
         $('.home-tab' + mobileText).hide();
         $('.register-tab' + mobileText).show();
     };

    document.getElementById("check-button" + mobileText).onclick = function(){
        $('.home-tab' + mobileText).hide();
        $('.check-tab' + mobileText).show();
    };

    document.getElementById("submit-register-button" + mobileText).onclick = function(){
        registerLedger(mobileText);
    };

    document.getElementById("submit-check-button" + mobileText).onclick = function(){
        checkLedger(mobileText);
    };

    $(".go-back-button" + mobileText).click(function(){
        $('.home-tab' + mobileText).show();
        $('.register-tab' + mobileText).hide();
        $('.check-tab' + mobileText).hide();
        document.getElementById("check-result" + mobileText).innerHTML = "Result:";
    });

});
