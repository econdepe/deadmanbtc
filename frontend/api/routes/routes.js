var appRouter = function (app) {
    app.get("/", function(req, res) {
      res.status(200).send("Welcome to our restful API");
    });

    //receive information from form
    app.post("/register", function(req, res) {

        //send to ledger
        var publicKey = req.body.publickey;
        var timeperiod = req.body.timeperiod;
        var currenthash = req.body.currenthash;
        var password = req.body.password;

    });

    app.get("/check", function(req, res) {
        res.status(200).send(JSON.stringify({privatekey: 'E9873D79C6D87DC0FB6A5778633389F4453213303DA61F20BD67FC233AA33262', status: 0}));

    });
  };

  module.exports = appRouter;
