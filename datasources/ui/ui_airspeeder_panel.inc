		<Page
			BackgroundColor='#2D2D2D'
			BackgroundOpacity='0.95'
			Location='804,400'
			MinimumSize='240,240'
			Name='AirspeederPanel'
			PackLocation='fff,nfn'
			PackSize='f,f'
			RStyleDefault='/Styles.New.tool.tool_tip.rs_default'
			ScrollExtent='256,240'
			Size='256,240'
			UserMovable='true'
			Visible='false'
		>
			<Data
				buttonAutoPilotCancel='buttonAutoPilotCancel'
				buttonBoost='buttonBoost'
				buttonHorn='buttonHorn'
				buttonSkyway='buttonSkyway'
				buttonTraffic='buttonTraffic'
				textAutoPilotStatus='textAutoPilotStatus'
				Name='CodeData'
			/>
			<Page
				BackgroundColor='#1E1E1E'
				BackgroundOpacity='1.00'
				MinimumSize='0,28'
				Name='header'
				PackLocation='nfn,nfn'
				PackSize='a,f'
				RStyleDefault='/Styles.New.tool.tool_tip.rs_default'
				ScrollExtent='256,28'
				Size='256,28'
			>
				<Text
					Font='bold_13'
					LocalText='Skyway Controls'
					Location='12,6'
					Name='labelTitle'
					ScrollExtent='232,18'
					Size='232,18'
					TextColor='#E8E8E8'
				>Skyway Controls</Text>
				<Page
					BackgroundColor='#505050'
					BackgroundOpacity='1.00'
					Location='8,26'
					Name='lineSeparator'
					PackLocation='nfn,nfn'
					PackSize='a,f'
					ScrollExtent='240,1'
					Size='240,1'
				/>
			</Page>
			<Button
				LocalText='Enter Skyway'
				Location='12,36'
				Name='buttonSkyway'
				ScrollExtent='232,26'
				Size='232,26'
				Style='/Styles.New.buttons.hud.style'
			>Enter Skyway</Button>
			<Button
				LocalText='Boost Mode'
				Location='12,68'
				Name='buttonBoost'
				ScrollExtent='232,26'
				Size='232,26'
				Style='/Styles.New.buttons.hud.style'
			>Boost Mode</Button>
			<Button
				LocalText='Traffic Mode'
				Location='12,100'
				Name='buttonTraffic'
				ScrollExtent='232,26'
				Size='232,26'
				Style='/Styles.New.buttons.hud.style'
			>Traffic Mode</Button>
			<Text
				Font='bold_13'
				LocalText=''
				Location='12,134'
				Name='textAutoPilotStatus'
				ScrollExtent='232,16'
				Size='232,16'
				TextColor='#90EE90'
				Visible='false'
			></Text>
			<Button
				LocalText='Cancel Auto-Pilot'
				Location='12,154'
				Name='buttonAutoPilotCancel'
				ScrollExtent='160,26'
				Size='160,26'
				Style='/Styles.New.buttons.hud.style'
				Visible='false'
			>Cancel Auto-Pilot</Button>
			<Button
				LocalText='Horn'
				Location='176,208'
				Name='buttonHorn'
				ScrollExtent='68,24'
				Size='68,24'
				Style='/Styles.New.buttons.hud.style'
			>Horn</Button>
		</Page>
