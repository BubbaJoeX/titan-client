		<Page
			BackgroundColor='#333333'
			BackgroundOpacity='0.90'
			Location='804,548'
			MinimumSize='220,400'
			Name='DecoratorCameraPanel'
			PackLocation='fff,fff'
			PackSize='f,f'
			RStyleDefault='/Styles.New.tool.tool_tip.rs_default'
			ScrollExtent='240,420'
			Size='240,420'
			UserMovable='true'
			UserResizable='false'
			Visible='false'
		>
			<Data
				buttonDropTerrain='buttonDropTerrain'
				buttonHelp='buttonHelp'
				buttonResetSelection='buttonResetSelection'
				buttonSpawning='buttonSpawning'
				checkDragOnTerrain='checkDragOnTerrain'
				checkFocusCamera='checkFocusCamera'
				checkGimbalMovement='checkGimbalMovement'
				checkMouse4Drag='checkMouse4Drag'
				checkSnapHorizontal='checkSnapHorizontal'
				checkSnapVertical='checkSnapVertical'
				Name='CodeData'
				textPosition='textPosition'
				textSelection='textSelection'
				textSnapMeters='textSnapMeters'
			/>
			<Text
				Font='bold_12'
				LocalText='Decorator Options'
				Location='8,6'
				Name='labelTitle'
				ScrollExtent='200,16'
				Size='200,16'
				TextColor='#FFFFFF'
			>Decorator Options</Text>
			<Checkbox
				Color='#96F4FC'
				Location='10,28'
				Name='checkSnapHorizontal'
				ScrollExtent='180,20'
				Size='180,20'
				Style='/Styles.New.Checkbox.Style2'
			>Snap horizontal (J)</Checkbox>
			<Checkbox
				Color='#96F4FC'
				Location='10,48'
				Name='checkSnapVertical'
				ScrollExtent='180,20'
				Size='180,20'
				Style='/Styles.New.Checkbox.Style2'
			>Snap vertical (H)</Checkbox>
			<Text
				Font='bold_11'
				LocalText='Grid: 1.0 m (G to cycle)'
				Location='10,70'
				Name='textSnapMeters'
				ScrollExtent='200,14'
				Size='200,14'
				TextColor='#CCCCCC'
			>Grid: 1.0 m (G to cycle)</Text>
			<Checkbox
				Color='#96F4FC'
				Location='10,88'
				Name='checkFocusCamera'
				ScrollExtent='180,20'
				Size='180,20'
				Style='/Styles.New.Checkbox.Style2'
			>Focus Camera on Selection (F)</Checkbox>
			<Checkbox
				Color='#96F4FC'
				Location='10,108'
				Name='checkGimbalMovement'
				ScrollExtent='180,20'
				Size='180,20'
				Style='/Styles.New.Checkbox.Style2'
			>Gimbal (Mouse 1)</Checkbox>
			<Checkbox
				Color='#96F4FC'
				Location='10,128'
				Name='checkMouse4Drag'
				ScrollExtent='180,20'
				Size='180,20'
				Style='/Styles.New.Checkbox.Style2'
			>Mouse4 Free Drag</Checkbox>
			<Checkbox
				Color='#96F4FC'
				Location='10,148'
				Name='checkDragOnTerrain'
				ScrollExtent='180,20'
				Size='180,20'
				Style='/Styles.New.Checkbox.Style2'
			>Drag on Terrain (Y)</Checkbox>
			<Button
				LocalText='Drop to Terrain'
				Location='10,172'
				Name='buttonDropTerrain'
				ScrollExtent='200,22'
				Size='200,22'
				Style='/Styles.New.buttons.hud.style'
			>Drop to Terrain</Button>
			<Button
				LocalText='Reset Selection (Face North)'
				Location='10,198'
				Name='buttonResetSelection'
				ScrollExtent='200,22'
				Size='200,22'
				Style='/Styles.New.buttons.hud.style'
			>Reset Selection (Face North)</Button>
			<Button
				LocalText='Help'
				Location='10,224'
				Name='buttonHelp'
				ScrollExtent='200,22'
				Size='200,22'
				Style='/Styles.New.buttons.hud.style'
			>Help</Button>
			<Button
				LocalText='Spawning'
				Location='10,250'
				Name='buttonSpawning'
				ScrollExtent='200,22'
				Size='200,22'
				Style='/Styles.New.buttons.hud.style'
			>Spawning</Button>
			<Text
				Font='bold_11'
				Location='10,278'
				MaxLines='1'
				Name='textPosition'
				ScrollExtent='220,14'
				Size='220,14'
				TextColor='#CCCCCC'
			>X: 0.00  Z: 0.00  Y: 0.00</Text>
			<Text
				Font='bold_11'
				Location='10,294'
				MaxLines='2'
				Name='textSelection'
				ScrollExtent='220,28'
				Size='220,28'
				TextColor='#CCCCCC'
			>Selection: (none)</Text>
		</Page>
