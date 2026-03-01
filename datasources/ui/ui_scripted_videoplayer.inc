	<Page
		Location='302,194'
		MinimumSize='420,450'
		Name='videoPlayer'
		ScrollExtent='420,450'
		Size='420,450'
		UserMovable='true'
		UserResizable='false'
		Visible='false'
	>
		<Data
			buttonclose='bg.mmc.close'
			lblTime='comp.sliderSection.lblTime'
			Name='CodeData'
			slider='comp.sliderSection.slider'
		/>

		<!-- Main Content -->
		<Composite
			Location='15,40'
			Name='comp'
			PackLocation='nfn,nfn'
			PackSize='a,a'
			Padding='Both'
			ScrollExtent='390,360'
			Size='390,360'
			Spacing='6'
			SpacingType='Constant'
		>
			<!-- Status Section -->
			<Page
				BackgroundOpacity='1.00'
				BackgroundTint='#1CFFFF'
				MinimumSize='0,110'
				Name='statusSection'
				PackSize='a,f'
				PalBgTint='line1'
				RStyleDefault='/Styles.New.whiteOutline.rs_default'
				ScrollExtent='390,110'
				Size='390,110'
			>
				<Text
					Enabled='false'
					Font='bold_11'
					LocalText='Status: STOPPED'
					Location='10,8'
					Name='lblStatus'
					PackLocation='nfn,nfn'
					PalText='text1'
					ScrollExtent='370,16'
					Size='370,16'
					TextColor='#FF4444'
				>Status: STOPPED</Text>

				<Text
					Enabled='false'
					Font='bold_10'
					LocalText='URL: (none)'
					Location='10,28'
					Name='lblUrl'
					PackLocation='nfn,nfn'
					PalText='text1'
					ScrollExtent='370,16'
					Size='370,16'
					TextColor='#96F4FC'
				>URL: (none)</Text>

				<Text
					Enabled='false'
					Font='bold_10'
					LocalText='Loop: Off'
					Location='10,48'
					Name='lblLoop'
					PackLocation='nfn,nfn'
					PalText='text1'
					ScrollExtent='180,16'
					Size='180,16'
					TextColor='#96F4FC'
				>Loop: Off</Text>

				<Text
					Enabled='false'
					Font='bold_10'
					LocalText='Aspect: 4:3'
					Location='200,48'
					Name='lblAspect'
					PackLocation='nfn,nfn'
					PalText='text1'
					ScrollExtent='180,16'
					Size='180,16'
					TextColor='#96F4FC'
				>Aspect: 4:3</Text>

				<Text
					Enabled='false'
					Font='bold_10'
					LocalText='Elapsed: -'
					Location='10,68'
					Name='lblElapsed'
					PackLocation='nfn,nfn'
					PalText='text1'
					ScrollExtent='180,16'
					Size='180,16'
					TextColor='#96F4FC'
				>Elapsed: -</Text>

				<Text
					Enabled='false'
					Font='bold_10'
					LocalText='ID: -'
					Location='200,68'
					Name='lblId'
					PackLocation='nfn,nfn'
					PalText='text1'
					ScrollExtent='180,16'
					Size='180,16'
					TextColor='#96F4FC'
				>ID: -</Text>

				<Text
					Enabled='false'
					Font='bold_10'
					LocalText=''
					Location='10,88'
					Name='lblName'
					PackLocation='nfn,nfn'
					PalText='text1'
					ScrollExtent='370,16'
					Size='370,16'
					TextColor='#FFFF00'
				></Text>

				<Page
					BackgroundColor='#FFFFFF'
					BackgroundOpacity='0.50'
					BackgroundTint='#00D6FB'
					Enabled='false'
					Location='4,4'
					Name='box'
					PackLocation='nfn,nfn'
					PackSize='a,a'
					PalBgTint='back1'
					RStyleDefault='/Styles.New.whitebox_palW.rs_default'
					ScrollExtent='382,102'
					Size='382,102'
				/>
			</Page>

			<!-- Playback Controls -->
			<Page
				BackgroundOpacity='1.00'
				BackgroundTint='#1CFFFF'
				MinimumSize='0,40'
				Name='playbackControls'
				PackSize='a,f'
				PalBgTint='line1'
				RStyleDefault='/Styles.New.whiteOutline.rs_default'
				ScrollExtent='390,40'
				Size='390,40'
			>
				<Button
					LocalText='Play'
					Location='10,10'
					Name='btnPlay'
					PackLocation='nfn,cfc'
					PackSize='p,f'
					ScrollExtent='60,22'
					Size='60,22'
					Style='/Styles.New.buttons.hud.style'
					TextColor='#000000'
				>Play</Button>

				<Button
					LocalText='Pause'
					Location='78,10'
					Name='btnPause'
					PackLocation='nfn,cfc'
					PackSize='p,f'
					ScrollExtent='60,22'
					Size='60,22'
					Style='/Styles.New.buttons.hud.style'
					TextColor='#000000'
				>Pause</Button>

				<Button
					LocalText='Stop'
					Location='146,10'
					Name='btnStop'
					PackLocation='nfn,cfc'
					PackSize='p,f'
					ScrollExtent='60,22'
					Size='60,22'
					Style='/Styles.New.buttons.hud.style'
					TextColor='#000000'
				>Stop</Button>

				<Button
					LocalText='Loop: Off'
					Location='214,10'
					Name='btnLoop'
					PackLocation='nfn,cfc'
					PackSize='p,f'
					ScrollExtent='80,22'
					Size='80,22'
					Style='/Styles.New.buttons.hud.style'
					TextColor='#000000'
				>Loop: Off</Button>

				<Button
					LocalText='Aspect: 4:3'
					Location='302,10'
					Name='btnAspect'
					PackLocation='nfn,cfc'
					PackSize='p,f'
					ScrollExtent='80,22'
					Size='80,22'
					Style='/Styles.New.buttons.hud.style'
					TextColor='#000000'
				>Aspect: 4:3</Button>

				<Page
					BackgroundColor='#FFFFFF'
					BackgroundOpacity='0.50'
					BackgroundTint='#00D6FB'
					Enabled='false'
					Location='4,4'
					Name='box'
					PackLocation='nfn,nfn'
					PackSize='a,a'
					PalBgTint='back1'
					RStyleDefault='/Styles.New.whitebox_palW.rs_default'
					ScrollExtent='382,32'
					Size='382,32'
				/>
			</Page>

			<!-- Seek Slider and Time Display -->
			<Page
				BackgroundOpacity='1.00'
				BackgroundTint='#1CFFFF'
				MinimumSize='0,50'
				Name='sliderSection'
				PackSize='a,f'
				PalBgTint='line1'
				RStyleDefault='/Styles.New.whiteOutline.rs_default'
				ScrollExtent='390,50'
				Size='390,50'
			>
				<Sliderbar
					Location='10,6'
					Name='slider'
					PackLocation='nfn,nfn'
					PackSize='a,p'
					ScrollExtent='370,28'
					Size='370,28'
					Style='/Styles.New.Slider.default.calib.style_test'
					LowerLimit='0'
					UpperLimit='100'
					Value='0'
				>
				</Sliderbar>

				<Text
					Enabled='false'
					Font='bold_10'
					LocalText='00:00:00 / 00:00:00'
					Location='10,32'
					Name='lblTime'
					PackLocation='nfn,nfn'
					PalText='text1'
					ScrollExtent='370,14'
					Size='370,14'
					TextColor='#96F4FC'
					TextAlignmentHorizontal='Center'
				>00:00:00 / 00:00:00</Text>

				<Page
					BackgroundColor='#FFFFFF'
					BackgroundOpacity='0.50'
					BackgroundTint='#00D6FB'
					Enabled='false'
					Location='4,4'
					Name='box'
					PackLocation='nfn,nfn'
					PackSize='a,a'
					PalBgTint='back1'
					RStyleDefault='/Styles.New.whitebox_palW.rs_default'
					ScrollExtent='382,42'
					Size='382,42'
				/>
			</Page>

			<!-- Action Buttons -->
			<Page
				BackgroundOpacity='1.00'
				BackgroundTint='#1CFFFF'
				MinimumSize='0,40'
				Name='actionControls'
				PackSize='a,f'
				PalBgTint='line1'
				RStyleDefault='/Styles.New.whiteOutline.rs_default'
				ScrollExtent='390,40'
				Size='390,40'
			>
				<Button
					LocalText='Set URL'
					Location='10,10'
					Name='btnSetUrl'
					PackLocation='nfn,cfc'
					PackSize='p,f'
					ScrollExtent='110,22'
					Size='110,22'
					Style='/Styles.New.buttons.hud.style'
					TextColor='#000000'
				>Set URL</Button>

				<Button
					LocalText='Set Timestamp'
					Location='130,10'
					Name='btnSetTimestamp'
					PackLocation='nfn,cfc'
					PackSize='p,f'
					ScrollExtent='110,22'
					Size='110,22'
					Style='/Styles.New.buttons.hud.style'
					TextColor='#000000'
				>Set Timestamp</Button>

				<Button
					LocalText='Spawn Speaker'
					Location='250,10'
					Name='btnSpawnSpeaker'
					PackLocation='nfn,cfc'
					PackSize='p,f'
					ScrollExtent='130,22'
					Size='130,22'
					Style='/Styles.New.buttons.hud.style'
					TextColor='#000000'
				>Spawn Speaker</Button>

				<Page
					BackgroundColor='#FFFFFF'
					BackgroundOpacity='0.50'
					BackgroundTint='#00D6FB'
					Enabled='false'
					Location='4,4'
					Name='box'
					PackLocation='nfn,nfn'
					PackSize='a,a'
					PalBgTint='back1'
					RStyleDefault='/Styles.New.whitebox_palW.rs_default'
					ScrollExtent='382,32'
					Size='382,32'
				/>
			</Page>

			<!-- Feedback Label -->
			<Page
				BackgroundOpacity='1.00'
				BackgroundTint='#1CFFFF'
				MinimumSize='0,30'
				Name='feedbackSection'
				PackSize='a,f'
				PalBgTint='line1'
				RStyleDefault='/Styles.New.whiteOutline.rs_default'
				ScrollExtent='390,30'
				Size='390,30'
			>
				<Text
					Enabled='false'
					Font='bold_10'
					LocalText=''
					Location='10,7'
					Name='lblFeedback'
					PackLocation='nfn,cfc'
					PackSize='a,f'
					PalText='text1'
					ScrollExtent='370,16'
					Size='370,16'
					TextColor='#00FF00'
				></Text>

				<Page
					BackgroundColor='#FFFFFF'
					BackgroundOpacity='0.50'
					BackgroundTint='#00D6FB'
					Enabled='false'
					Location='4,4'
					Name='box'
					PackLocation='nfn,nfn'
					PackSize='a,a'
					PalBgTint='back1'
					RStyleDefault='/Styles.New.whitebox_palW.rs_default'
					ScrollExtent='382,22'
					Size='382,22'
				/>
			</Page>
		</Composite>

		<!-- Close Button -->
		<Button
			IsCancelButton='true'
			LocalText='Close'
			Location='325,418'
			MinimumSize='0,24'
			Name='btnClose'
			PackLocation='fff,fff'
			PackSize='p,f'
			ScrollExtent='80,24'
			Size='80,24'
			Style='/Styles.New.buttons.hud.style'
			TextColor='#000000'
		>Close</Button>

		<!-- Background Frame -->
		<Page
			Name='bg'
			PackLocation='nfn,nfn'
			PackSize='a,a'
			ScrollExtent='420,450'
			Size='420,450'
		>
			<!-- Close X Button -->
			<Page
				Location='408,8'
				Name='mmc'
				PackLocation='fff,nfn'
				PackSize='f,f'
				RStyleDefault='/Styles.New.thin_mmc.rs_default'
				ScrollExtent='12,10'
				Size='12,10'
			>
				<Button
					BackgroundColor='#FFFFFF'
					BackgroundOpacity='0.80'
					IsCancelButton='true'
					Location='-4,-3'
					Name='close'
					PackLocation='fff,cfc'
					PackSize='f,f'
					ScrollExtent='16,16'
					Size='16,16'
					Style='/Styles.Frame.buttonClose.style'
				></Button>
			</Page>

			<!-- Title Bar -->
			<Page
				MinimumSize='0,24'
				Name='caption'
				PackLocation='nfn,nfn'
				PackSize='a,f'
				ScrollExtent='420,24'
				Size='420,24'
			>
				<Text
					Enabled='false'
					Font='bold_14'
					LocalText='Video Player'
					Margin='8,-2,0,0'
					MaxLines='1'
					MinimumSize='0,24'
					Name='lblTitle'
					PackSize='a,f'
					PalText='titletext'
					ScrollExtent='420,24'
					Size='420,24'
					TextAlignmentVertical='Center'
					TextCapital='true'
					TextColor='#00354F'
				>Video Player</Text>
			</Page>

			<!-- Frame Background -->
			<Page
				BackgroundTint='#00D6FB'
				GetsInput='false'
				Location='1,0'
				Name='frame'
				PackLocation='nfn,nfn'
				PackSize='a,a'
				PalBgTint='baselight'
				RStyleDefault='/Styles.New.thin_in.rs_default'
				ScrollExtent='419,450'
				Selectable='true'
				Size='419,450'
			>
				<!-- Title Bar Background -->
				<Page
					BackgroundOpacity='1.00'
					Location='-1,0'
					Name='top'
					PackLocation='nfn,nfn'
					PackSize='a,f'
					RStyleDefault='/Styles.New.main_TR.rs_default'
					ScrollExtent='420,34'
					Size='420,34'
				/>
				<!-- Bottom Bar Background -->
				<Page
					BackgroundOpacity='1.00'
					Location='0,427'
					Name='bottom'
					PackLocation='nfn,fff'
					PackSize='a,f'
					RStyleDefault='/Styles.New.main_BR.rs_default'
					ScrollExtent='419,23'
					Size='419,23'
				/>
			</Page>
			<!-- Main Background -->
			<Page
				BackgroundOpacity='1.00'
				BackgroundTint='#00D6FB'
				GetsInput='false'
				Location='0,5'
				Name='back'
				PackLocation='nfn,nfn'
				PackSize='a,a'
				PalBgTint='back1'
				RStyleDefault='/Styles.New.thin_in.rs_default'
				ScrollExtent='420,442'
				Selectable='true'
				Size='420,442'
			/>
			<!-- Glow Background -->
			<Page
				BackgroundOpacity='0.80'
				BackgroundTint='#00D6FB'
				GetsInput='false'
				Name='back2'
				PackLocation='nfn,nfn'
				PackSize='a,a'
				PalBgTint='back1'
				RStyleDefault='/Styles.space.spaceglow.rs_default'
				ScrollExtent='420,450'
				Selectable='true'
				Size='420,450'
			/>
		</Page>
	</Page>
