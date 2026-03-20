function toColor(value) 
{
	return "#" + (Math.round(value * 0XFFFFFF)).toString(16);
}

window.onload = function() 
{
		var fileInput = document.getElementById('fileInput');
		var eventList = document.getElementById('elementList');
		var eventCount = document.getElementById("eventCount");
		var canvas = document.getElementById("canvas");
		
		fileInput.addEventListener('change', function(e) {
			var file = fileInput.files[0];
			var textType = /text.*/;

			var reader = new FileReader();

			reader.onload = function(e)
			{
				var jsonText = reader.result;
				var jsonObject = JSON.parse(jsonText);

				// Write header
				eventHeader.innerHTML = 'Profiler Version: ' + jsonObject.version + "<br>";
			 	eventHeader.innerHTML += 'Total Event Count: ' + jsonObject.eventCount + "<br>";
				eventHeader.innerHTML += 'Events Written: ' + jsonObject.eventsWritten + "<br>";
				eventHeader.innerHTML += 'CPU Count: ' + jsonObject.cpuCount + "<br>";
				if (jsonObject.categories)
				{
					eventHeader.innerHTML += 'Categories:';
					eventHeader.innerHTML += '<ol>';
					for (i=0; i<jsonObject.categories.length; i++)
					{
						eventHeader.innerHTML += '<li>[' + i + '] = ' + jsonObject.categories[i];
					}
					eventHeader.innerHTML += '</ol>';
				}
				if (jsonObject.eventNames)
				{
					eventHeader.innerHTML += '<br>Event Names:';
					eventHeader.innerHTML += '<ol>';
					for (i=0; i<jsonObject.eventNames.length; i++)
					{
						eventHeader.innerHTML += '<li>[' + i + '] = ' + jsonObject.eventNames[i];
					}
					eventHeader.innerHTML += '</ol>';
				}

				var i;
				var eventTableInput = jsonObject.events;
      			var eventTable = "<table border=1>";

				// Table header items are the key labels
				eventTable += "<tr>";

				var haveDuration = false;
				var haveColorId = false;
				var categoriesArray = [];
				var categoriesColors = [];
				var categoryKey = "";
				if (eventTableInput.length)
				{
					eventRow =  eventTableInput[0];
					// Example to sort by duration descending 
					if ("duration" in eventRow)
					{
						haveDuration = true;
						eventTableInput.sort(
							function(a,b)
							{
								return b.duration-a.duration;
							});
					}
					if ("colorId" in eventRow)
					{
						haveColorId = true;
					}
					
					for (var key in eventRow)
					{
						eventTable += "<td><b>" + key.toUpperCase() + "</b></td>";
					}
			
					var categories = new Set();
					for(i = 0; i < eventTableInput.length; i++)		
					{
						eventRow =  eventTableInput[i];
						if ("catIdx" in eventRow)
						{
							categories.add(eventRow['catIdx'])
							categoryKey = "catIdx";
						}
						else if ("category" in eventRow)
						{
							categories.add(eventRow['category'])
							categoryKey = "category";
						}
					}
					categories.forEach(v => categoriesArray.push(v));
					categories.forEach(v => categoriesColors.push(Math.random()));
					console.log("Categories: " + categoriesArray);
					console.log("Categories: " + categoriesColors);					
				}
				eventTable += "</tr>";

				// Row items are key data
				var durationData = [];	
				var durationTotal = 0.0;
				var durationColors = [];
				for(i = 0; i < eventTableInput.length; i++)
				{
					eventRow =  eventTableInput[i];

					if (haveDuration)
					{
						duration = eventRow['duration'];
						durationTotal += duration;
						if (categoriesArray.length && categoryKey.length)
						{
							catIdx = categoriesArray.indexOf(eventRow[categoryKey]);
							console.log('cat: ' + eventRow[categoryKey] + '. cat idx: ' + catIdx + "/" + categoriesArray.length + 'cat color: ' + categoriesColors[catIdx]);
							//durationColor = toColor(categoriesColors[catIdx] * (catIdx / categoriesArray.length));
							durationColor = toColor((catIdx+1) / (categoriesArray.length+1));
						}
						else
						{
							durationColor = toColor(Math.random());
						}
						durationData.push(duration);
						durationColors.push(durationColor);
					}
					
					eventTable += "<tr>"
					for (var key in eventRow)
					{
						if (key == 'duration')
						{
							var x = "<td bgcolor=\"" + durationColor + "\"><font color=\"#FFFFFF\">" + eventRow[key] + "</font></td>";	
							console.log(x);
							eventTable += x;							
						}
						else
						{
							eventTable += "<td>" + eventRow[key] + "</td>";
						}
					}
					eventTable += "</tr>"
				}
      			eventTable += "</table>";
      			eventList.innerHTML = eventTable;
				
				// Draw a pie chart
				if (canvas)
				{
					var ctx = canvas.getContext("2d");
					
					ctx.clearRect(0, 0, canvas.width, canvas.height);
					if (durationData.length)
					{				
						var center = [canvas.width / 2, canvas.height / 2];
						var radius = Math.min(canvas.width, canvas.height) / 2;
						var lastPosition = 0, total = 0;
						
						for (var i = 0; i < durationData.length; i++) 
						{
							ctx.fillStyle = durationColors[i];
							ctx.beginPath();
							ctx.moveTo(center[0],center[1]);
							ctx.arc(center[0],center[1],
									radius,
									lastPosition,lastPosition+(Math.PI*2*(durationData[i]/durationTotal)),
									false);
							ctx.lineTo(center[0],center[1]);
							ctx.fill();
							lastPosition += Math.PI*2*(durationData[i]/durationTotal);
						}
					}
				}
			}

			reader.readAsText(file);
		});
}

// SIG // Begin signature block
// SIG // MIIpZAYJKoZIhvcNAQcCoIIpVTCCKVECAQExDzANBglg
// SIG // hkgBZQMEAgEFADB3BgorBgEEAYI3AgEEoGkwZzAyBgor
// SIG // BgEEAYI3AgEeMCQCAQEEEBDgyQbOONQRoqMAEEvTUJAC
// SIG // AQACAQACAQACAQACAQAwMTANBglghkgBZQMEAgEFAAQg
// SIG // gpNxeoT4NXJxm6GO53SPKzboNiiJHnv7NJoh16lX8Fyg
// SIG // gg4cMIIGsDCCBJigAwIBAgIQCK1AsmDSnEyfXs2pvZOu
// SIG // 2TANBgkqhkiG9w0BAQwFADBiMQswCQYDVQQGEwJVUzEV
// SIG // MBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
// SIG // d3cuZGlnaWNlcnQuY29tMSEwHwYDVQQDExhEaWdpQ2Vy
// SIG // dCBUcnVzdGVkIFJvb3QgRzQwHhcNMjEwNDI5MDAwMDAw
// SIG // WhcNMzYwNDI4MjM1OTU5WjBpMQswCQYDVQQGEwJVUzEX
// SIG // MBUGA1UEChMORGlnaUNlcnQsIEluYy4xQTA/BgNVBAMT
// SIG // OERpZ2lDZXJ0IFRydXN0ZWQgRzQgQ29kZSBTaWduaW5n
// SIG // IFJTQTQwOTYgU0hBMzg0IDIwMjEgQ0ExMIICIjANBgkq
// SIG // hkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA1bQvQtAorXi3
// SIG // XdU5WRuxiEL1M4zrPYGXcMW7xIUmMJ+kjmjYXPXrNCQH
// SIG // 4UtP03hD9BfXHtr50tVnGlJPDqFX/IiZwZHMgQM+TXAk
// SIG // ZLON4gh9NH1MgFcSa0OamfLFOx/y78tHWhOmTLMBICXz
// SIG // ENOLsvsI8IrgnQnAZaf6mIBJNYc9URnokCF4RS6hnyzh
// SIG // GMIazMXuk0lwQjKP+8bqHPNlaJGiTUyCEUhSaN4QvRRX
// SIG // XegYE2XFf7JPhSxIpFaENdb5LpyqABXRN/4aBpTCfMjq
// SIG // GzLmysL0p6MDDnSlrzm2q2AS4+jWufcx4dyt5Big2MEj
// SIG // R0ezoQ9uo6ttmAaDG7dqZy3SvUQakhCBj7A7CdfHmzJa
// SIG // wv9qYFSLScGT7eG0XOBv6yb5jNWy+TgQ5urOkfW+0/tv
// SIG // k2E0XLyTRSiDNipmKF+wc86LJiUGsoPUXPYVGUztYuBe
// SIG // M/Lo6OwKp7ADK5GyNnm+960IHnWmZcy740hQ83eRGv7b
// SIG // UKJGyGFYmPV8AhY8gyitOYbs1LcNU9D4R+Z1MI3sMJN2
// SIG // FKZbS110YU0/EpF23r9Yy3IQKUHw1cVtJnZoEUETWJrc
// SIG // JisB9IlNWdt4z4FKPkBHX8mBUHOFECMhWWCKZFTBzCEa
// SIG // 6DgZfGYczXg4RTCZT/9jT0y7qg0IU0F8WD1Hs/q27Iwy
// SIG // CQLMbDwMVhECAwEAAaOCAVkwggFVMBIGA1UdEwEB/wQI
// SIG // MAYBAf8CAQAwHQYDVR0OBBYEFGg34Ou2O/hfEYb7/mF7
// SIG // CIhl9E5CMB8GA1UdIwQYMBaAFOzX44LScV1kTN8uZz/n
// SIG // upiuHA9PMA4GA1UdDwEB/wQEAwIBhjATBgNVHSUEDDAK
// SIG // BggrBgEFBQcDAzB3BggrBgEFBQcBAQRrMGkwJAYIKwYB
// SIG // BQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBB
// SIG // BggrBgEFBQcwAoY1aHR0cDovL2NhY2VydHMuZGlnaWNl
// SIG // cnQuY29tL0RpZ2lDZXJ0VHJ1c3RlZFJvb3RHNC5jcnQw
// SIG // QwYDVR0fBDwwOjA4oDagNIYyaHR0cDovL2NybDMuZGln
// SIG // aWNlcnQuY29tL0RpZ2lDZXJ0VHJ1c3RlZFJvb3RHNC5j
// SIG // cmwwHAYDVR0gBBUwEzAHBgVngQwBAzAIBgZngQwBBAEw
// SIG // DQYJKoZIhvcNAQEMBQADggIBADojRD2NCHbuj7w6mdNW
// SIG // 4AIapfhINPMstuZ0ZveUcrEAyq9sMCcTEp6QRJ9L/Z6j
// SIG // fCbVN7w6XUhtldU/SfQnuxaBRVD9nL22heB2fjdxyyL3
// SIG // WqqQz/WTauPrINHVUHmImoqKwba9oUgYftzYgBoRGRjN
// SIG // YZmBVvbJ43bnxOQbX0P4PpT/djk9ntSZz0rdKOtfJqGV
// SIG // WEjVGv7XJz/9kNF2ht0csGBc8w2o7uCJob054ThO2m67
// SIG // Np375SFTWsPK6Wrxoj7bQ7gzyE84FJKZ9d3OVG3ZXQIU
// SIG // H0AzfAPilbLCIXVzUstG2MQ0HKKlS43Nb3Y3LIU/Gs4m
// SIG // 6Ri+kAewQ3+ViCCCcPDMyu/9KTVcH4k4Vfc3iosJocsL
// SIG // 6TEa/y4ZXDlx4b6cpwoG1iZnt5LmTl/eeqxJzy6kdJKt
// SIG // 2zyknIYf48FWGysj/4+16oh7cGvmoLr9Oj9FpsToFpFS
// SIG // i0HASIRLlk2rREDjjfAVKM7t8RhWByovEMQMCGQ8M4+u
// SIG // KIw8y4+ICw2/O/TOHnuO77Xry7fwdxPm5yg/rBKupS8i
// SIG // bEH5glwVZsxsDsrFhsP2JjMMB0ug0wcCampAMEhLNKhR
// SIG // ILutG4UI4lkNbcoFUCvqShyepf2gpx8GdOfy1lKQ/a+F
// SIG // SCH5Vzu0nAPthkX0tGFuv2jiJmCG6sivqf6UHedjGzqG
// SIG // VnhOMIIHZDCCBUygAwIBAgIQCML5hAyQ3mOJNr1Fvmvp
// SIG // 5TANBgkqhkiG9w0BAQsFADBpMQswCQYDVQQGEwJVUzEX
// SIG // MBUGA1UEChMORGlnaUNlcnQsIEluYy4xQTA/BgNVBAMT
// SIG // OERpZ2lDZXJ0IFRydXN0ZWQgRzQgQ29kZSBTaWduaW5n
// SIG // IFJTQTQwOTYgU0hBMzg0IDIwMjEgQ0ExMB4XDTI1MDEy
// SIG // MTAwMDAwMFoXDTI2MDEyMTIzNTk1OVowbDELMAkGA1UE
// SIG // BhMCVVMxEzARBgNVBAgTCkNhbGlmb3JuaWExFjAUBgNV
// SIG // BAcTDVNhbiBGcmFuY2lzY28xFzAVBgNVBAoTDkF1dG9k
// SIG // ZXNrLCBJbmMuMRcwFQYDVQQDEw5BdXRvZGVzaywgSW5j
// SIG // LjCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIB
// SIG // AK74NlCBkOyAE+1GkdZokyCS9inzUZsA0P0DgoyX/+NS
// SIG // SPPuI3ru/l0p3dXpLIxFNGvf8mhR0nM5nuOV3nd2ZIK3
// SIG // Jixw0UR63P7JDnORJ3ln8smaUjfb2lGf1WAJkBb8gpJr
// SIG // plYJ29FJguJ7HClc3wY8dCnh3bvn1ivTvUawV9y8ttpN
// SIG // Nl0pwp2SDSacmh73a26JdU6sBiK668uNa4/MVWmnhst2
// SIG // aMQeC/gWJnCuk/LT+a52cGvVYr5IQPRFhhxucI1IOduc
// SIG // 5bFs/yvxL0es2IuFp3YYmfnG/C9isCR4zAMr4jt3LrOk
// SIG // JwQkBaPNnErISKKOEJdnCKImTcdS8u3ObPvYU9oI1JyS
// SIG // Bs+1rGAL7VZgeqtIJGocADNqVDqNWCxIE61cx/zWQmq/
// SIG // rshp3VgY0nIcc8f1j1dt4idPk/ax6BxXUaIZJ16lhh6g
// SIG // WLMiw6yzNHLMV5WD0YBfAkBMIUlv/yJVnyLe9S/dwRxE
// SIG // +Vx2HqGd0qj20hXdGCv3L3qEh/YRZAjBeu33lPeve27Z
// SIG // DvKNv1Nyqpre3owXr6FPWaxbpCt4Gs+x1PS6Q+n+9eX+
// SIG // t+L4AKABPKVTkAT5AzQJk7aBhqRghpF7VGgyQ/mAc+I1
// SIG // zQCA2c+TgCXwolYF2PCVNmtsjvthULp7dywoERCjvKh6
// SIG // mgno5SXimy9ZHz3B7Xy5YJSlAgMBAAGjggIDMIIB/zAf
// SIG // BgNVHSMEGDAWgBRoN+Drtjv4XxGG+/5hewiIZfROQjAd
// SIG // BgNVHQ4EFgQUrA5FAvOuY1WDqamWsiRsOIhRK9owPgYD
// SIG // VR0gBDcwNTAzBgZngQwBBAEwKTAnBggrBgEFBQcCARYb
// SIG // aHR0cDovL3d3dy5kaWdpY2VydC5jb20vQ1BTMA4GA1Ud
// SIG // DwEB/wQEAwIHgDATBgNVHSUEDDAKBggrBgEFBQcDAzCB
// SIG // tQYDVR0fBIGtMIGqMFOgUaBPhk1odHRwOi8vY3JsMy5k
// SIG // aWdpY2VydC5jb20vRGlnaUNlcnRUcnVzdGVkRzRDb2Rl
// SIG // U2lnbmluZ1JTQTQwOTZTSEEzODQyMDIxQ0ExLmNybDBT
// SIG // oFGgT4ZNaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL0Rp
// SIG // Z2lDZXJ0VHJ1c3RlZEc0Q29kZVNpZ25pbmdSU0E0MDk2
// SIG // U0hBMzg0MjAyMUNBMS5jcmwwgZQGCCsGAQUFBwEBBIGH
// SIG // MIGEMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdp
// SIG // Y2VydC5jb20wXAYIKwYBBQUHMAKGUGh0dHA6Ly9jYWNl
// SIG // cnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydFRydXN0ZWRH
// SIG // NENvZGVTaWduaW5nUlNBNDA5NlNIQTM4NDIwMjFDQTEu
// SIG // Y3J0MAkGA1UdEwQCMAAwDQYJKoZIhvcNAQELBQADggIB
// SIG // ACK7fmVjRXuvBRO/52u0hRHPBaHHY0ZHljL/nEl/Al51
// SIG // NvEyKH6TK18vnka4gf4RgLO0t+geDVvM+sA+HkF+Sfpq
// SIG // glahId98lsZDWrxL10WX9mYd0aK92rL0KnWPhPCuj1en
// SIG // fykv/JeXsWpduRv9Y+t8G2wXvU4qI39x3K1F3ySu2PAZ
// SIG // hWHRMIbYE1YXwD21oqiZYPwRQj2oBiv8Sxq+lEd6TSZh
// SIG // +AwURh6SWRXW7YfnL1eAKtq7B2ynVCU+QyfeU/k5yXwC
// SIG // I8ZhnzsyEerI56rpDwZtUC4BOEySV0FZieHu5rx9hpDx
// SIG // 3HFmvrmT+H3epT9QILh+hm6zGbxYSxkuitfxc5XvsoMv
// SIG // pd64srvxYlgIXvSxGBp6UHC+9SKtpjwRBgRuJcw41+ca
// SIG // w8VTvnFUn5M99WmFJwacW82xELhOqePiqykm5LrCmUQx
// SIG // nAi5ElktGaumiE6QOOxc0ZncxdPPCRH+2+c+FaOJc6Rc
// SIG // a/upA2QpRG9gQcpupEkvuheD4Lb2D3yUa9HKufs4Qmt3
// SIG // FujiMHo1CuHTfgB2ZutoHQaVCArIV3GfART6AE6sla98
// SIG // /GWxlZuMVt/jMkEnyY2HdkDkBuXZpzveOeEwBYStGNmg
// SIG // YK5FArFRYPweG++E5hRREWskNU2/sP7IMx/6q9R1H3vu
// SIG // taZtwtfHKdK1QYIYTUWH2E2fMYIaoDCCGpwCAQEwfTBp
// SIG // MQswCQYDVQQGEwJVUzEXMBUGA1UEChMORGlnaUNlcnQs
// SIG // IEluYy4xQTA/BgNVBAMTOERpZ2lDZXJ0IFRydXN0ZWQg
// SIG // RzQgQ29kZSBTaWduaW5nIFJTQTQwOTYgU0hBMzg0IDIw
// SIG // MjEgQ0ExAhAIwvmEDJDeY4k2vUW+a+nlMA0GCWCGSAFl
// SIG // AwQCAQUAoHwwEAYKKwYBBAGCNwIBDDECMAAwGQYJKoZI
// SIG // hvcNAQkDMQwGCisGAQQBgjcCAQQwHAYKKwYBBAGCNwIB
// SIG // CzEOMAwGCisGAQQBgjcCARUwLwYJKoZIhvcNAQkEMSIE
// SIG // INlfe1fNFlz+dMEzGkGqx5HA8PNr34n/7MmbPSztquDw
// SIG // MA0GCSqGSIb3DQEBAQUABIICABr6yYqjWAFmJbUsDU5d
// SIG // vEbST/M6/MOFLj/n25s3aq2D7KHPwWSEghg1daoO0CPg
// SIG // gZ3mURG+OJ7A/QuhyTEqBhIoOrumM7iI9MW54thiv/z6
// SIG // c9MHW0wh1K5zcTasPWhab4holjWJFjMY0gOMS47BfIUR
// SIG // QRB9tJV2MVQBflShrBk+ew7jVugpHvv8IWwmoSn/fTGP
// SIG // B5WSTU76Qh6RJyyjCsuWhXGcAf3Z1BXenEs/7d8DorLf
// SIG // 7tAi0yNFR5EeFsByiLoyVpV9WYEx3GZOyqB8o8MEh5J2
// SIG // C/rUTnSZQwuXRax3B8jD/9Wt1GcTuwc17kko8vUqdHxv
// SIG // AEFP3xmX+EIkHR3yl9a3IilUgjo+X1f0+p8yAIKCkF5l
// SIG // A+98YOnHBgAmx4LpiQESW4g/FwjuoH3ZLWZWnchDQdK1
// SIG // jYGWb7tGbx+wBZGQ5m7fhGNu8MIHa1HGKPEc3Sj98qc3
// SIG // b4j9pTssvp9j6u4uGWld1r03PRWDp16cKvwW9cioxZyf
// SIG // mQ/QcgdX7lMF0po6YE21cqBi6PZAiVCGy3BW/maGTSdi
// SIG // tL6ob8jW3WZM1ProDor7EzZFDmFvM+FG8tLmuRhqxdrg
// SIG // 486DjZXZaIT/KKhVQ6+GQOuIJB/T1Dl6lZ4ovU33iPbX
// SIG // Xlh+FYGXa9vcGfac/VQEgCG+9ShWQVmnUPZJb/N5E7ZL
// SIG // lsYkoYIXdjCCF3IGCisGAQQBgjcDAwExghdiMIIXXgYJ
// SIG // KoZIhvcNAQcCoIIXTzCCF0sCAQMxDzANBglghkgBZQME
// SIG // AgEFADB3BgsqhkiG9w0BCRABBKBoBGYwZAIBAQYJYIZI
// SIG // AYb9bAcBMDEwDQYJYIZIAWUDBAIBBQAEIF85fzifimY4
// SIG // hiuGoFeLCRlJ3UkZDzmBNOrb1617FJtdAhAAhDipznUc
// SIG // LMnZhaywT04QGA8yMDI1MTAyOTE5NTM0MlqgghM6MIIG
// SIG // 7TCCBNWgAwIBAgIQCoDvGEuN8QWC0cR2p5V0aDANBgkq
// SIG // hkiG9w0BAQsFADBpMQswCQYDVQQGEwJVUzEXMBUGA1UE
// SIG // ChMORGlnaUNlcnQsIEluYy4xQTA/BgNVBAMTOERpZ2lD
// SIG // ZXJ0IFRydXN0ZWQgRzQgVGltZVN0YW1waW5nIFJTQTQw
// SIG // OTYgU0hBMjU2IDIwMjUgQ0ExMB4XDTI1MDYwNDAwMDAw
// SIG // MFoXDTM2MDkwMzIzNTk1OVowYzELMAkGA1UEBhMCVVMx
// SIG // FzAVBgNVBAoTDkRpZ2lDZXJ0LCBJbmMuMTswOQYDVQQD
// SIG // EzJEaWdpQ2VydCBTSEEyNTYgUlNBNDA5NiBUaW1lc3Rh
// SIG // bXAgUmVzcG9uZGVyIDIwMjUgMTCCAiIwDQYJKoZIhvcN
// SIG // AQEBBQADggIPADCCAgoCggIBANBGrC0Sxp7Q6q5gVrMr
// SIG // V7pvUf+GcAoB38o3zBlCMGMyqJnfFNZx+wvA69HFTBdw
// SIG // bHwBSOeLpvPnZ8ZN+vo8dE2/pPvOx/Vj8TchTySA2R4Q
// SIG // KpVD7dvNZh6wW2R6kSu9RJt/4QhguSssp3qome7MrxVy
// SIG // fQO9sMx6ZAWjFDYOzDi8SOhPUWlLnh00Cll8pjrUcCV3
// SIG // K3E0zz09ldQ//nBZZREr4h/GI6Dxb2UoyrN0ijtUDVHR
// SIG // XdmncOOMA3CoB/iUSROUINDT98oksouTMYFOnHoRh6+8
// SIG // 6Ltc5zjPKHW5KqCvpSduSwhwUmotuQhcg9tw2YD3w6yS
// SIG // SSu+3qU8DD+nigNJFmt6LAHvH3KSuNLoZLc1Hf2JNMVL
// SIG // 4Q1OpbybpMe46YceNA0LfNsnqcnpJeItK/DhKbPxTTuG
// SIG // oX7wJNdoRORVbPR1VVnDuSeHVZlc4seAO+6d2sC26/PQ
// SIG // PdP51ho1zBp+xUIZkpSFA8vWdoUoHLWnqWU3dCCyFG1r
// SIG // oSrgHjSHlq8xymLnjCbSLZ49kPmk8iyyizNDIXj//cOg
// SIG // rY7rlRyTlaCCfw7aSUROwnu7zER6EaJ+AliL7ojTdS5P
// SIG // WPsWeupWs7NpChUk555K096V1hE0yZIXe+giAwW00aHz
// SIG // rDchIc2bQhpp0IoKRR7YufAkprxMiXAJQ1XCmnCfgPf8
// SIG // +3mnAgMBAAGjggGVMIIBkTAMBgNVHRMBAf8EAjAAMB0G
// SIG // A1UdDgQWBBTkO/zyMe39/dfzkXFjGVBDz2GM6DAfBgNV
// SIG // HSMEGDAWgBTvb1NK6eQGfHrK4pBW9i/USezLTjAOBgNV
// SIG // HQ8BAf8EBAMCB4AwFgYDVR0lAQH/BAwwCgYIKwYBBQUH
// SIG // AwgwgZUGCCsGAQUFBwEBBIGIMIGFMCQGCCsGAQUFBzAB
// SIG // hhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wXQYIKwYB
// SIG // BQUHMAKGUWh0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNv
// SIG // bS9EaWdpQ2VydFRydXN0ZWRHNFRpbWVTdGFtcGluZ1JT
// SIG // QTQwOTZTSEEyNTYyMDI1Q0ExLmNydDBfBgNVHR8EWDBW
// SIG // MFSgUqBQhk5odHRwOi8vY3JsMy5kaWdpY2VydC5jb20v
// SIG // RGlnaUNlcnRUcnVzdGVkRzRUaW1lU3RhbXBpbmdSU0E0
// SIG // MDk2U0hBMjU2MjAyNUNBMS5jcmwwIAYDVR0gBBkwFzAI
// SIG // BgZngQwBBAIwCwYJYIZIAYb9bAcBMA0GCSqGSIb3DQEB
// SIG // CwUAA4ICAQBlKq3xHCcEua5gQezRCESeY0ByIfjk9iJP
// SIG // 2zWLpQq1b4URGnwWBdEZD9gBq9fNaNmFj6Eh8/YmRDfx
// SIG // T7C0k8FUFqNh+tshgb4O6Lgjg8K8elC4+oWCqnU/ML9l
// SIG // Ffim8/9yJmZSe2F8AQ/UdKFOtj7YMTmqPO9mzskgiC3Q
// SIG // YIUP2S3HQvHG1FDu+WUqW4daIqToXFE/JQ/EABgfZXLW
// SIG // U0ziTN6R3ygQBHMUBaB5bdrPbF6MRYs03h4obEMnxYOX
// SIG // 8VBRKe1uNnzQVTeLni2nHkX/QqvXnNb+YkDFkxUGtMTa
// SIG // iLR9wjxUxu2hECZpqyU1d0IbX6Wq8/gVutDojBIFeRlq
// SIG // AcuEVT0cKsb+zJNEsuEB7O7/cuvTQasnM9AWcIQfVjnz
// SIG // rvwiCZ85EE8LUkqRhoS3Y50OHgaY7T/lwd6UArb+BOVA
// SIG // kg2oOvol/DJgddJ35XTxfUlQ+8Hggt8l2Yv7roancJIF
// SIG // cbojBcxlRcGG0LIhp6GvReQGgMgYxQbV1S3CrWqZzBt1
// SIG // R9xJgKf47CdxVRd/ndUlQ05oxYy2zRWVFjF7mcr4C34M
// SIG // j3ocCVccAvlKV9jEnstrniLvUxxVZE/rptb7IRE2lskK
// SIG // PIJgbaP5t2nGj/ULLi49xTcBZU8atufk+EMF/cWuiC7P
// SIG // OGT75qaL6vdCvHlshtjdNXOCIUjsarfNZzCCBrQwggSc
// SIG // oAMCAQICEA3HrFcF/yGZLkBDIgw6SYYwDQYJKoZIhvcN
// SIG // AQELBQAwYjELMAkGA1UEBhMCVVMxFTATBgNVBAoTDERp
// SIG // Z2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3LmRpZ2ljZXJ0
// SIG // LmNvbTEhMB8GA1UEAxMYRGlnaUNlcnQgVHJ1c3RlZCBS
// SIG // b290IEc0MB4XDTI1MDUwNzAwMDAwMFoXDTM4MDExNDIz
// SIG // NTk1OVowaTELMAkGA1UEBhMCVVMxFzAVBgNVBAoTDkRp
// SIG // Z2lDZXJ0LCBJbmMuMUEwPwYDVQQDEzhEaWdpQ2VydCBU
// SIG // cnVzdGVkIEc0IFRpbWVTdGFtcGluZyBSU0E0MDk2IFNI
// SIG // QTI1NiAyMDI1IENBMTCCAiIwDQYJKoZIhvcNAQEBBQAD
// SIG // ggIPADCCAgoCggIBALR4MdMKmEFyvjxGwBysddujRmh0
// SIG // tFEXnU2tjQ2UtZmWgyxU7UNqEY81FzJsQqr5G7A6c+Gh
// SIG // /qm8Xi4aPCOo2N8S9SLrC6Kbltqn7SWCWgzbNfiR+2fk
// SIG // HUiljNOqnIVD/gG3SYDEAd4dg2dDGpeZGKe+42DFUF0m
// SIG // R/vtLa4+gKPsYfwEu7EEbkC9+0F2w4QJLVSTEG8yAR2C
// SIG // QWIM1iI5PHg62IVwxKSpO0XaF9DPfNBKS7Zazch8NF5v
// SIG // p7eaZ2CVNxpqumzTCNSOxm+SAWSuIr21Qomb+zzQWKhx
// SIG // KTVVgtmUPAW35xUUFREmDrMxSNlr/NsJyUXzdtFUUt4a
// SIG // S4CEeIY8y9IaaGBpPNXKFifinT7zL2gdFpBP9qh8SdLn
// SIG // Eut/GcalNeJQ55IuwnKCgs+nrpuQNfVmUB5KlCX3ZA4x
// SIG // 5HHKS+rqBvKWxdCyQEEGcbLe1b8Aw4wJkhU1JrPsFfxW
// SIG // 1gaou30yZ46t4Y9F20HHfIY4/6vHespYMQmUiote8lad
// SIG // jS/nJ0+k6MvqzfpzPDOy5y6gqztiT96Fv/9bH7mQyogx
// SIG // G9QEPHrPV6/7umw052AkyiLA6tQbZl1KhBtTasySkuJD
// SIG // psZGKdlsjg4u70EwgWbVRSX1Wd4+zoFpp4Ra+MlKM2ba
// SIG // oD6x0VR4RjSpWM8o5a6D8bpfm4CLKczsG7ZrIGNTAgMB
// SIG // AAGjggFdMIIBWTASBgNVHRMBAf8ECDAGAQH/AgEAMB0G
// SIG // A1UdDgQWBBTvb1NK6eQGfHrK4pBW9i/USezLTjAfBgNV
// SIG // HSMEGDAWgBTs1+OC0nFdZEzfLmc/57qYrhwPTzAOBgNV
// SIG // HQ8BAf8EBAMCAYYwEwYDVR0lBAwwCgYIKwYBBQUHAwgw
// SIG // dwYIKwYBBQUHAQEEazBpMCQGCCsGAQUFBzABhhhodHRw
// SIG // Oi8vb2NzcC5kaWdpY2VydC5jb20wQQYIKwYBBQUHMAKG
// SIG // NWh0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNvbS9EaWdp
// SIG // Q2VydFRydXN0ZWRSb290RzQuY3J0MEMGA1UdHwQ8MDow
// SIG // OKA2oDSGMmh0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9E
// SIG // aWdpQ2VydFRydXN0ZWRSb290RzQuY3JsMCAGA1UdIAQZ
// SIG // MBcwCAYGZ4EMAQQCMAsGCWCGSAGG/WwHATANBgkqhkiG
// SIG // 9w0BAQsFAAOCAgEAF877FoAc/gc9EXZxML2+C8i1NKZ/
// SIG // zdCHxYgaMH9Pw5tcBnPw6O6FTGNpoV2V4wzSUGvI9NAz
// SIG // aoQk97frPBtIj+ZLzdp+yXdhOP4hCFATuNT+ReOPK0mC
// SIG // efSG+tXqGpYZ3essBS3q8nL2UwM+NMvEuBd/2vmdYxDC
// SIG // vwzJv2sRUoKEfJ+nN57mQfQXwcAEGCvRR2qKtntujB71
// SIG // WPYAgwPyWLKu6RnaID/B0ba2H3LUiwDRAXx1Neq9ydOa
// SIG // l95CHfmTnM4I+ZI2rVQfjXQA1WSjjf4J2a7jLzWGNqNX
// SIG // +DF0SQzHU0pTi4dBwp9nEC8EAqoxW6q17r0z0noDjs6+
// SIG // BFo+z7bKSBwZXTRNivYuve3L2oiKNqetRHdqfMTCW/Nm
// SIG // KLJ9M+MtucVGyOxiDf06VXxyKkOirv6o02OoXN4bFzK0
// SIG // vlNMsvhlqgF2puE6FndlENSmE+9JGYxOGLS/D284NHNb
// SIG // oDGcmWXfwXRy4kbu4QFhOm0xJuF2EZAOk5eCkhSxZON3
// SIG // rGlHqhpB/8MluDezooIs8CVnrpHMiD2wL40mm53+/j7t
// SIG // FaxYKIqL0Q4ssd8xHZnIn/7GELH3IdvG2XlM9q7WP/Uw
// SIG // gOkw/HQtyRN62JK4S1C8uw3PdBunvAZapsiI5YKdvlar
// SIG // Evf8EA+8hcpSM9LHJmyrxaFtoza2zNaQ9k+5t1wwggWN
// SIG // MIIEdaADAgECAhAOmxiO+dAt5+/bUOIIQBhaMA0GCSqG
// SIG // SIb3DQEBDAUAMGUxCzAJBgNVBAYTAlVTMRUwEwYDVQQK
// SIG // EwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdp
// SIG // Y2VydC5jb20xJDAiBgNVBAMTG0RpZ2lDZXJ0IEFzc3Vy
// SIG // ZWQgSUQgUm9vdCBDQTAeFw0yMjA4MDEwMDAwMDBaFw0z
// SIG // MTExMDkyMzU5NTlaMGIxCzAJBgNVBAYTAlVTMRUwEwYD
// SIG // VQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5k
// SIG // aWdpY2VydC5jb20xITAfBgNVBAMTGERpZ2lDZXJ0IFRy
// SIG // dXN0ZWQgUm9vdCBHNDCCAiIwDQYJKoZIhvcNAQEBBQAD
// SIG // ggIPADCCAgoCggIBAL/mkHNo3rvkXUo8MCIwaTPswqcl
// SIG // LskhPfKK2FnC4SmnPVirdprNrnsbhA3EMB/zG6Q4FutW
// SIG // xpdtHauyefLKEdLkX9YFPFIPUh/GnhWlfr6fqVcWWVVy
// SIG // r2iTcMKyunWZanMylNEQRBAu34LzB4TmdDttceItDBvu
// SIG // INXJIB1jKS3O7F5OyJP4IWGbNOsFxl7sWxq868nPzaw0
// SIG // QF+xembud8hIqGZXV59UWI4MK7dPpzDZVu7Ke13jrclP
// SIG // XuU15zHL2pNe3I6PgNq2kZhAkHnDeMe2scS1ahg4AxCN
// SIG // 2NQ3pC4FfYj1gj4QkXCrVYJBMtfbBHMqbpEBfCFM1Lyu
// SIG // GwN1XXhm2ToxRJozQL8I11pJpMLmqaBn3aQnvKFPObUR
// SIG // WBf3JFxGj2T3wWmIdph2PVldQnaHiZdpekjw4KISG2aa
// SIG // dMreSx7nDmOu5tTvkpI6nj3cAORFJYm2mkQZK37AlLTS
// SIG // YW3rM9nF30sEAMx9HJXDj/chsrIRt7t/8tWMcCxBYKqx
// SIG // YxhElRp2Yn72gLD76GSmM9GJB+G9t+ZDpBi4pncB4Q+U
// SIG // DCEdslQpJYls5Q5SUUd0viastkF13nqsX40/ybzTQRES
// SIG // W+UQUOsxxcpyFiIJ33xMdT9j7CFfxCBRa2+xq4aLT8LW
// SIG // RV+dIPyhHsXAj6KxfgommfXkaS+YHS312amyHeUbAgMB
// SIG // AAGjggE6MIIBNjAPBgNVHRMBAf8EBTADAQH/MB0GA1Ud
// SIG // DgQWBBTs1+OC0nFdZEzfLmc/57qYrhwPTzAfBgNVHSME
// SIG // GDAWgBRF66Kv9JLLgjEtUYunpyGd823IDzAOBgNVHQ8B
// SIG // Af8EBAMCAYYweQYIKwYBBQUHAQEEbTBrMCQGCCsGAQUF
// SIG // BzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wQwYI
// SIG // KwYBBQUHMAKGN2h0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0
// SIG // LmNvbS9EaWdpQ2VydEFzc3VyZWRJRFJvb3RDQS5jcnQw
// SIG // RQYDVR0fBD4wPDA6oDigNoY0aHR0cDovL2NybDMuZGln
// SIG // aWNlcnQuY29tL0RpZ2lDZXJ0QXNzdXJlZElEUm9vdENB
// SIG // LmNybDARBgNVHSAECjAIMAYGBFUdIAAwDQYJKoZIhvcN
// SIG // AQEMBQADggEBAHCgv0NcVec4X6CjdBs9thbX979XB72a
// SIG // rKGHLOyFXqkauyL4hxppVCLtpIh3bb0aFPQTSnovLbc4
// SIG // 7/T/gLn4offyct4kvFIDyE7QKt76LVbP+fT3rDB6mouy
// SIG // XtTP0UNEm0Mh65ZyoUi0mcudT6cGAxN3J0TU53/oWajw
// SIG // vy8LpunyNDzs9wPHh6jSTEAZNUZqaVSwuKFWjuyk1T3o
// SIG // sdz9HNj0d1pcVIxv76FQPfx2CWiEn2/K2yCNNWAcAgPL
// SIG // ILCsWKAOQGPFmCLBsln1VWvPJ6tsds5vIy30fnFqI2si
// SIG // /xK4VC0nftg62fC2h5b9W9FcrBjDTZ9ztwGpn1eqXiji
// SIG // uZQxggN8MIIDeAIBATB9MGkxCzAJBgNVBAYTAlVTMRcw
// SIG // FQYDVQQKEw5EaWdpQ2VydCwgSW5jLjFBMD8GA1UEAxM4
// SIG // RGlnaUNlcnQgVHJ1c3RlZCBHNCBUaW1lU3RhbXBpbmcg
// SIG // UlNBNDA5NiBTSEEyNTYgMjAyNSBDQTECEAqA7xhLjfEF
// SIG // gtHEdqeVdGgwDQYJYIZIAWUDBAIBBQCggdEwGgYJKoZI
// SIG // hvcNAQkDMQ0GCyqGSIb3DQEJEAEEMBwGCSqGSIb3DQEJ
// SIG // BTEPFw0yNTEwMjkxOTUzNDJaMCsGCyqGSIb3DQEJEAIM
// SIG // MRwwGjAYMBYEFN1iMKyGCi0wa9o4sWh5UjAH+0F+MC8G
// SIG // CSqGSIb3DQEJBDEiBCDgZkOwF2phPAAo9aZ9WbDwfeuX
// SIG // eL1khdyWBhNxXNK/0DA3BgsqhkiG9w0BCRACLzEoMCYw
// SIG // JDAiBCBKoD+iLNdchMVck4+CjmdrnK7Ksz/jbSaaozTx
// SIG // RhEKMzANBgkqhkiG9w0BAQEFAASCAgCn2Z/zSYq+xqIf
// SIG // geThSXWgHEjd4dv37+DXpgsoACeeIh8w6L3XPTX7M7d/
// SIG // ttLPi2Sxzz1QQ8Lo/VPLgPFD9RvpphByeIwjqEfD8j52
// SIG // ryOdQH9L74A7K2Yh17p5GIX2leBmfxQcoaYJYFc/U5Nn
// SIG // q0gWpfQQgedKn+uhraZsl9LFtxKju0Zs3TD1LviPQkDe
// SIG // MNp4qXrFf/TZNTnrqDAl9Lt5Fy0tGIpqJd30e949UETI
// SIG // VFSCwIPvctnTK1A6sVlAD9Lj7CRC+gGl2y+gnWLxvJ5Y
// SIG // nKkXsycRGQ5aV7/GGll3WqOr/+Si/06YuM+qR9HHxgwd
// SIG // utfIFiD99w3whhL+DbKHLrSFHQR2NKe0hashrPhefExq
// SIG // bDAomjhWQOVC74Ui5r5RMG8vB5/qJUrcnlKTz50xmeMb
// SIG // sl06uZI8kz6T7wjpJZYE/xqdF8ARrWa2bBPxfTlv4Wlt
// SIG // TVDtJk3+immkNMsmxXgOXyW9f6O8a43v9ZBLy/AmlT87
// SIG // ++1LexVoHQKNBdl4g1/bDxc4bRu0LwRNQPzjngB7tJFf
// SIG // Vt1376fNI+3mKnIzk9R+tdGrvVLnLTyAaM5ArMKdWNZe
// SIG // 53zCJAEBXXWo974f3rmGS6ht7YL7whVyQQlBKEb575UE
// SIG // a8VGtIWEuUTkBe3HED+OBNCsMdoiMrSMzXXF3MJ9WGzY
// SIG // wodS1AALqg==
// SIG // End signature block
